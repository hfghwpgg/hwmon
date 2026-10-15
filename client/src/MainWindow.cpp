#include "MainWindow.hpp"

#include "ClientBackend.hpp"
#include "MonitorModel.hpp"
#include "UiStyle.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSettings>
#include <QSize>
#include <QSpinBox>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

// Qt's built-in drag auto-scroll samples QCursor::pos(), which is not updated during a
// Wayland drag, so scrolling stops immediately. Drive it from drag move positions instead.
class DragScrollTreeView : public QTreeView {
public:
  explicit DragScrollTreeView(QWidget *parent = nullptr) :
      QTreeView(parent) {
    setAutoScroll(false);
    m_scrollTimer.setInterval(16);
    QObject::connect(&m_scrollTimer, &QTimer::timeout, this, [this] { scrollStep(); });
  }

protected:
  void dragMoveEvent(QDragMoveEvent *event) override {
    QTreeView::dragMoveEvent(event);
    m_dragPos = event->position().toPoint();
    m_dragActions = event->possibleActions();
    m_dragMime = event->mimeData();
    m_dragButtons = event->buttons();
    m_dragModifiers = event->modifiers();
    if (scrollSpeed() != 0.0) {
      if (!m_scrollTimer.isActive()) {
        m_scrollTimer.start();
      }
    } else {
      stopScroll();
    }
  }

  void dragLeaveEvent(QDragLeaveEvent *event) override {
    stopScroll();
    m_dragMime = nullptr;
    QTreeView::dragLeaveEvent(event);
  }

  void dropEvent(QDropEvent *event) override {
    stopScroll();
    m_dragMime = nullptr;
    QTreeView::dropEvent(event);
  }

private:
  // Signed fraction in [-1, 1] of how deep the drag position is inside the top/bottom margin.
  double scrollSpeed() const {
    const int margin = std::max(autoScrollMargin(), 2 * std::max(sizeHintForRow(0), 64));
    const int height = viewport()->height();
    const int y = m_dragPos.y();
    if (y < margin) {
      return -double(margin - std::max(y, 0)) / margin;
    }
    if (y > height - margin) {
      return double(std::min(y, height) - (height - margin)) / margin;
    }
    return 0.0;
  }

  void scrollStep() {
    const double speed = scrollSpeed();
    QScrollBar *bar = verticalScrollBar();
    const int before = bar->value();
    if (verticalScrollMode() == QAbstractItemView::ScrollPerPixel) {
      m_scrollRemainder += speed * 24.0;
    } else {
      m_scrollRemainder += speed * 0.6;
    }
    const int delta = int(std::trunc(m_scrollRemainder));
    m_scrollRemainder -= delta;
    bar->setValue(before + delta);
    if (speed == 0.0 || (delta != 0 && bar->value() == before)) {
      stopScroll();
      return;
    }
    if (bar->value() != before && m_dragMime) {
      // Refresh the drop indicator for the row that is now under the stationary pointer.
      QDragMoveEvent refresh(QPointF(m_dragPos), m_dragActions, m_dragMime, m_dragButtons,
                             m_dragModifiers);
      QTreeView::dragMoveEvent(&refresh);
    }
  }

  void stopScroll() {
    m_scrollTimer.stop();
    m_scrollRemainder = 0.0;
  }

  QTimer m_scrollTimer;
  QPoint m_dragPos;
  Qt::DropActions m_dragActions;
  const QMimeData *m_dragMime = nullptr;
  Qt::MouseButtons m_dragButtons;
  Qt::KeyboardModifiers m_dragModifiers;
  double m_scrollRemainder = 0.0;
};

} // namespace

MainWindow::MainWindow(ClientBackend &backend, QWidget *parent) :
    QMainWindow(parent),
    m_backend(backend),
    m_model(backend.model()) {
  setWindowTitle(QStringLiteral("hwmon"));
  resize(645, 760);
  setMinimumSize(420, 480);

  auto *central = new QWidget(this);
  auto *layout = new QVBoxLayout(central);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_tree = new DragScrollTreeView(central);
  m_tree->setModel(m_model);
  m_tree->setUniformRowHeights(true);
  m_tree->setAlternatingRowColors(true);
  m_tree->setRootIsDecorated(true);
  m_tree->setItemsExpandable(true);
  m_tree->setIconSize(QSize(16, 16));
  m_tree->setIndentation(22);
  m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_tree->setDragEnabled(true);
  m_tree->setAcceptDrops(true);
  m_tree->setDropIndicatorShown(true);
  m_tree->setDragDropMode(QAbstractItemView::InternalMove);
  m_tree->setDefaultDropAction(Qt::MoveAction);
  m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
  m_tree->setAllColumnsShowFocus(true);
  m_tree->header()->setStretchLastSection(false);
  m_tree->header()->setMinimumSectionSize(64);
  m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
  layout->addWidget(m_tree, 1);

  m_footer = new QWidget(central);
  m_footer->setFixedHeight(40);
  auto *footerLayout = new QHBoxLayout(m_footer);
  footerLayout->setContentsMargins(10, 6, 10, 6);

  m_statusLabel = new QLabel(m_footer);
  m_statusLabel->setMinimumWidth(110);
  m_statusLabel->setFont(sizedFont(m_statusLabel->font(), kFontStatus));
  m_elapsedLabel = new QLabel(m_footer);
  m_elapsedLabel->setAlignment(Qt::AlignCenter);
  QFont elapsedFont = m_elapsedLabel->font();
  elapsedFont.setBold(true);
  elapsedFont.setFamilies({QStringLiteral("monospace"), QStringLiteral("Noto Sans Mono")});
  m_elapsedLabel->setFont(
      sizedFont(elapsedFont, kFontElapsed > 0 ? kFontElapsed : elapsedFont.pointSize() + 2));

  m_resetButton = new QPushButton(QStringLiteral("Reset"), m_footer);
  auto *settingsButton = new QPushButton(QStringLiteral("Settings"), m_footer);

  footerLayout->addWidget(m_statusLabel);
  footerLayout->addWidget(m_elapsedLabel, 1);
  footerLayout->addWidget(m_resetButton);
  footerLayout->addWidget(settingsButton);
  layout->addWidget(m_footer);

  setCentralWidget(central);

  restoreHeaderState();
  restoreWindowState();
  applyTheme();
  updateFooter();

  connect(m_tree, &QTreeView::customContextMenuRequested, this, &MainWindow::showContextMenu);
  connect(m_tree, &QTreeView::expanded, this,
          [this](const QModelIndex &index) { m_model->setNodeExpanded(index, true); });
  connect(m_tree, &QTreeView::collapsed, this,
          [this](const QModelIndex &index) { m_model->setNodeExpanded(index, false); });
  connect(m_model, &QAbstractItemModel::modelReset, this, &MainWindow::restoreExpandedState);
  connect(m_resetButton, &QPushButton::clicked, &m_backend, &ClientBackend::resetReadings);
  connect(settingsButton, &QPushButton::clicked, this, &MainWindow::openSettings);
  connect(&m_backend, &ClientBackend::statusTextChanged, this, &MainWindow::updateFooter);
  connect(&m_backend, &ClientBackend::elapsedTextChanged, this, &MainWindow::updateFooter);
  connect(&m_backend, &ClientBackend::connectedChanged, this, &MainWindow::updateFooter);
  connect(&m_backend, &ClientBackend::darkModeChanged, this, [this] {
    applyTheme();
    updateFooter();
  });
  connect(m_tree->header(), &QHeaderView::sectionResized, this, [this] { saveHeaderState(); });
}

void MainWindow::closeEvent(QCloseEvent *event) {
  saveHeaderState();
  saveWindowState();
  QMainWindow::closeEvent(event);
}

void MainWindow::applyTheme() {
  const ThemeColors &t = themeColors(m_backend.darkMode());
  const auto hex = [](const QColor &color) { return color.name(); };
  // Placeholders: %1 text, %2 mutedText, %3 disabledText, %4 accent, %5 mainWindow,
  // %6 dialog, %7 treeBase, %8 alternateBase, %9 header, %10 border, %11 selection,
  // %12 control, %13 buttonHover, %14 window, %15 menuSelection, %16 radioBorder, %17 radioBase.
  static const QString qssTemplate =
      QStringLiteral("QMainWindow { background: %5; color: %1; }"
                     "QDialog { background: %6; color: %1; }"
                     "QTreeView { background: %7; alternate-background-color: %8;"
                     " color: %1; border: none; outline: none; }"
                     "QHeaderView::section { background: %9; color: %2;"
                     " padding: 6px; border: none; border-right: 1px solid %10;"
                     " font-weight: bold; }"
                     "QTreeView::item:selected { background: %11; color: %1; }"
                     "QPushButton { background: %12; color: %1; border: 1px solid %10;"
                     " border-radius: 4px; padding: 4px 12px; }"
                     "QPushButton:hover { background: %13; }"
                     "QPushButton:disabled { color: %3; }"
                     "QLabel { color: %1; background: transparent; }"
                     "QMenu { background: %14; color: %1; border: 1px solid %10; }"
                     "QMenu::item:selected { background: %15; }"
                     "QSpinBox { color: %1; background: %12; }"
                     "QRadioButton { color: %1; background: transparent; spacing: 8px; }"
                     "QRadioButton::indicator { width: 16px; height: 16px; border-radius: 9px;"
                     " border: 2px solid %16; background: %17; }"
                     "QRadioButton::indicator:hover { border-color: %4; }"
                     "QRadioButton::indicator:checked { border: 2px solid %4;"
                     " background: qradialgradient(cx:0.5, cy:0.5, radius:0.55, fx:0.5, fy:0.5,"
                     " stop:0 %4, stop:0.42 %4, stop:0.5 %17, stop:1 %17); }");
  setStyleSheet(
      qssTemplate
          .arg(hex(t.text), hex(t.mutedText), hex(t.disabledText), hex(t.accent), hex(t.mainWindow),
               hex(t.dialog), hex(t.treeBase), hex(t.alternateBase), hex(t.header))
          .arg(hex(t.border), hex(t.selection), hex(t.control), hex(t.buttonHover), hex(t.window),
               hex(t.menuSelection), hex(t.radioBorder), hex(t.radioBase)));
  m_footer->setStyleSheet(QStringLiteral("background: %1;").arg(hex(t.footer)));
}

void MainWindow::restoreExpandedState() {
  for (int row = 0; row < m_model->rowCount(); ++row) {
    const QModelIndex device = m_model->index(row, 0);
    m_tree->setExpanded(device, m_model->isNodeExpanded(device));
    for (int sectionRow = 0; sectionRow < m_model->rowCount(device); ++sectionRow) {
      const QModelIndex section = m_model->index(sectionRow, 0, device);
      m_tree->setExpanded(section, m_model->isNodeExpanded(section));
    }
  }
}

void MainWindow::updateFooter() {
  const bool active = m_backend.connected() || m_backend.fileMode();
  const ThemeColors &theme = themeColors(m_backend.darkMode());
  m_statusLabel->setText(m_backend.statusText());
  m_statusLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                   .arg((active ? theme.accent : theme.warning).name()));
  m_elapsedLabel->setText(m_backend.elapsedText());
  m_resetButton->setEnabled(active);
}

void MainWindow::showContextMenu(const QPoint &pos) {
  const QModelIndex index = m_tree->indexAt(pos).siblingAtColumn(0);
  if (!index.isValid()) {
    return;
  }

  QMenu menu(this);
  if (m_model->isSensor(index)) {
    QAction *hide = menu.addAction(QStringLiteral("Hide"));
    connect(hide, &QAction::triggered, this, [this, index] { m_model->hideSensor(index); });
  } else {
    QAction *collapse = menu.addAction(QStringLiteral("Collapse"));
    collapse->setEnabled(m_tree->isExpanded(index));
    connect(collapse, &QAction::triggered, this, [this, index] { m_tree->collapse(index); });

    QAction *expand = menu.addAction(QStringLiteral("Expand"));
    expand->setEnabled(!m_tree->isExpanded(index));
    connect(expand, &QAction::triggered, this, [this, index] { m_tree->expand(index); });

    QAction *showHidden = menu.addAction(QStringLiteral("Show hidden sensors"));
    showHidden->setEnabled(m_model->hasHiddenSensors(index));
    // showHiddenSensors resets the model; modelReset already restores expansion.
    connect(showHidden, &QAction::triggered, this,
            [this, index] { m_model->showHiddenSensors(index); });
  }
  menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void MainWindow::openSettings() {
  QDialog dialog(this);
  dialog.setWindowTitle(QStringLiteral("Settings"));
  dialog.setModal(true);
  dialog.resize(360, 240);

  auto *layout = new QVBoxLayout(&dialog);
  auto *form = new QFormLayout();

  auto *darkRadio = new QRadioButton(QStringLiteral("Dark"), &dialog);
  auto *lightRadio = new QRadioButton(QStringLiteral("Light"), &dialog);
  darkRadio->setChecked(m_backend.darkMode());
  lightRadio->setChecked(!m_backend.darkMode());

  auto *themeBox = new QWidget(&dialog);
  auto *themeLayout = new QVBoxLayout(themeBox);
  themeLayout->setContentsMargins(0, 0, 0, 0);
  themeLayout->addWidget(darkRadio);
  themeLayout->addWidget(lightRadio);
  form->addRow(QStringLiteral("Theme"), themeBox);

  auto *interval = new QSpinBox(&dialog);
  interval->setRange(50, 60000);
  interval->setSingleStep(50);
  interval->setSuffix(QStringLiteral(" ms"));
  interval->setValue(m_backend.intervalMs());
  form->addRow(QStringLiteral("Read interval"), interval);

  auto *resetOrder = new QPushButton(QStringLiteral("Reset sensor order"), &dialog);
  form->addRow(QString(), resetOrder);

  layout->addLayout(form);
  auto *hint = new QLabel(QStringLiteral("Minimum 50 ms. The same interval is sent to the server."),
                          &dialog);
  hint->setWordWrap(true);
  hint->setAutoFillBackground(true);
  // Keep the box at its natural height; spare dialog space goes to the stretch below.
  hint->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  auto applyHintStyle = [this, hint] {
    const ThemeColors &theme = themeColors(m_backend.darkMode());
    hint->setStyleSheet(
        QStringLiteral(
            "QLabel { background: %1; color: %2; padding: 2px 8px; border-radius: 4px; }")
            .arg(theme.hintBackground.name(), theme.hintText.name()));
  };
  applyHintStyle();
  layout->addWidget(hint);
  layout->addStretch(1);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

  connect(darkRadio, &QRadioButton::clicked, this, [this] { m_backend.setDarkMode(true); });
  connect(lightRadio, &QRadioButton::clicked, this, [this] { m_backend.setDarkMode(false); });
  connect(&m_backend, &ClientBackend::darkModeChanged, &dialog, applyHintStyle);
  connect(interval, &QSpinBox::valueChanged, this,
          [this](int value) { m_backend.setIntervalMs(value); });
  connect(resetOrder, &QPushButton::clicked, m_model, &MonitorModel::resetSensorOrder);

  dialog.exec();
}

void MainWindow::saveHeaderState() const {
  QSettings settings;
  settings.setValue(QStringLiteral("headerState"), m_tree->header()->saveState());
}

void MainWindow::restoreHeaderState() {
  const QByteArray state = QSettings().value(QStringLiteral("headerState")).toByteArray();
  if (!state.isEmpty()) {
    m_tree->header()->restoreState(state);
    return;
  }
  m_tree->setColumnWidth(MonitorModel::NameColumn, 220);
  for (int column = MonitorModel::CurrentColumn; column < MonitorModel::ColumnCount; ++column) {
    m_tree->setColumnWidth(column, 100);
  }
}

void MainWindow::saveWindowState() const {
  QSettings settings;
  settings.setValue(QStringLiteral("windowGeometry"), saveGeometry());
}

void MainWindow::restoreWindowState() {
  const QByteArray geometry = QSettings().value(QStringLiteral("windowGeometry")).toByteArray();
  if (!geometry.isEmpty()) {
    restoreGeometry(geometry);
  }
}
