#include "MainWindow.hpp"

#include "ClientBackend.hpp"
#include "MonitorModel.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QTreeView>
#include <QVBoxLayout>

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

  m_tree = new QTreeView(central);
  m_tree->setModel(m_model);
  m_tree->setUniformRowHeights(true);
  m_tree->setAlternatingRowColors(true);
  m_tree->setRootIsDecorated(true);
  m_tree->setItemsExpandable(true);
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
  m_elapsedLabel = new QLabel(m_footer);
  m_elapsedLabel->setAlignment(Qt::AlignCenter);
  QFont elapsedFont = m_elapsedLabel->font();
  elapsedFont.setBold(true);
  elapsedFont.setFamilies({QStringLiteral("monospace"), QStringLiteral("Noto Sans Mono")});
  elapsedFont.setPointSize(elapsedFont.pointSize() + 2);
  m_elapsedLabel->setFont(elapsedFont);

  m_resetButton = new QPushButton(QStringLiteral("Reset"), m_footer);
  auto *settingsButton = new QPushButton(QStringLiteral("Settings"), m_footer);

  footerLayout->addWidget(m_statusLabel);
  footerLayout->addWidget(m_elapsedLabel, 1);
  footerLayout->addWidget(m_resetButton);
  footerLayout->addWidget(settingsButton);
  layout->addWidget(m_footer);

  setCentralWidget(central);

  m_model->setDarkTheme(m_backend.darkMode());
  m_model->setFlashDurationMs(m_backend.intervalMs());
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
    m_model->setDarkTheme(m_backend.darkMode());
    applyTheme();
  });
  connect(&m_backend, &ClientBackend::intervalMsChanged, this,
          [this] { m_model->setFlashDurationMs(m_backend.intervalMs()); });
  connect(m_tree->header(), &QHeaderView::sectionResized, this, [this] { saveHeaderState(); });
}

void MainWindow::closeEvent(QCloseEvent *event) {
  saveHeaderState();
  saveWindowState();
  QMainWindow::closeEvent(event);
}

void MainWindow::applyTheme() {
  const bool dark = m_backend.darkMode();
  const QString qss =
      dark ? QStringLiteral(
                 "QMainWindow { background: #141414; color: #ececec; }"
                 "QDialog { background: #2a2a2a; color: #ececec; }"
                 "QTreeView { background: #1b1b1b; alternate-background-color: #222222;"
                 " color: #ececec; border: none; outline: none; }"
                 "QHeaderView::section { background: #2a2a2a; color: #9aa3ad;"
                 " padding: 6px; border: none; border-right: 1px solid #3a3a3a;"
                 " font-weight: bold; }"
                 "QTreeView::item:selected { background: #2d3d4e; color: #ececec; }"
                 "QPushButton { background: #333333; color: #ececec; border: 1px solid #3a3a3a;"
                 " border-radius: 4px; padding: 4px 12px; }"
                 "QPushButton:hover { background: #404040; }"
                 "QPushButton:disabled { color: #66707a; }"
                 "QLabel { color: #ececec; background: transparent; }"
                 "QMenu { background: #1e1e1e; color: #ececec; border: 1px solid #3a3a3a; }"
                 "QMenu::item:selected { background: #2d3d4e; }"
                 "QSpinBox { color: #ececec; background: #333333; }"
                 "QRadioButton { color: #ececec; background: transparent; spacing: 8px; }"
                 "QRadioButton::indicator { width: 16px; height: 16px; border-radius: 9px;"
                 " border: 2px solid #8a8a8a; background: #1a1a1a; }"
                 "QRadioButton::indicator:hover { border-color: #6cb6ff; }"
                 "QRadioButton::indicator:checked { border: 2px solid #6cb6ff;"
                 " background: qradialgradient(cx:0.5, cy:0.5, radius:0.55, fx:0.5, fy:0.5,"
                 " stop:0 #6cb6ff, stop:0.42 #6cb6ff, stop:0.5 #1a1a1a, stop:1 #1a1a1a); }")
           : QStringLiteral(
                 "QMainWindow { background: #f4f4f4; color: #1a1a1a; }"
                 "QDialog { background: #ffffff; color: #1a1a1a; }"
                 "QTreeView { background: #fafafa; alternate-background-color: #f0f0f0;"
                 " color: #1a1a1a; border: none; outline: none; }"
                 "QHeaderView::section { background: #e6e6e6; color: #66707a;"
                 " padding: 6px; border: none; border-right: 1px solid #cfcfcf;"
                 " font-weight: bold; }"
                 "QTreeView::item:selected { background: #c9dbeb; color: #1a1a1a; }"
                 "QPushButton { background: #ffffff; color: #1a1a1a; border: 1px solid #cfcfcf;"
                 " border-radius: 4px; padding: 4px 12px; }"
                 "QPushButton:hover { background: #f0f0f0; }"
                 "QPushButton:disabled { color: #9a9a9a; }"
                 "QLabel { color: #1a1a1a; background: transparent; }"
                 "QMenu { background: #ffffff; color: #1a1a1a; border: 1px solid #cfcfcf; }"
                 "QMenu::item:selected { background: #d7e4f2; }"
                 "QSpinBox { color: #1a1a1a; background: #ffffff; }"
                 "QRadioButton { color: #1a1a1a; background: transparent; spacing: 8px; }"
                 "QRadioButton::indicator { width: 16px; height: 16px; border-radius: 9px;"
                 " border: 2px solid #6a6a6a; background: #ffffff; }"
                 "QRadioButton::indicator:hover { border-color: #1565c0; }"
                 "QRadioButton::indicator:checked { border: 2px solid #1565c0;"
                 " background: qradialgradient(cx:0.5, cy:0.5, radius:0.55, fx:0.5, fy:0.5,"
                 " stop:0 #1565c0, stop:0.42 #1565c0, stop:0.5 #ffffff, stop:1 #ffffff); }");
  setStyleSheet(qss);
  if (m_footer != nullptr) {
    m_footer->setStyleSheet(dark ? QStringLiteral("background: #202020;")
                                 : QStringLiteral("background: #ececec;"));
  }
}

void MainWindow::restoreExpandedState()
{
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
  m_statusLabel->setText(m_backend.statusText());
  const QColor accent = m_backend.darkMode() ? QColor("#6cb6ff") : QColor("#1565c0");
  const QColor muted = m_backend.darkMode() ? QColor("#9aa3ad") : QColor("#66707a");
  QPalette palette = m_statusLabel->palette();
  palette.setColor(QPalette::WindowText,
                   m_backend.connected() || m_backend.fileMode() ? accent : muted);
  m_statusLabel->setPalette(palette);
  m_elapsedLabel->setText(m_backend.elapsedText());
  m_resetButton->setEnabled(m_backend.connected() || m_backend.fileMode());
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
        connect(showHidden, &QAction::triggered, this, [this, index] {
            m_model->showHiddenSensors(index);
            restoreExpandedState();
        });
    }
  menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void MainWindow::openSettings() {
  QDialog dialog(this);
  dialog.setWindowTitle(QStringLiteral("Settings"));
  dialog.setModal(true);
  dialog.resize(360, 280);

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
  auto applyHintStyle = [this, hint] {
    hint->setStyleSheet(
        m_backend.darkMode()
            ? QStringLiteral("QLabel { background: #3a3a3a; color: #d0d0d0; padding: 8px;"
                             " border-radius: 4px; }")
            : QStringLiteral("QLabel { background: #f3f3f3; color: #555555; padding: 8px;"
                             " border-radius: 4px; }"));
  };
  applyHintStyle();
  layout->addWidget(hint);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

  connect(darkRadio, &QRadioButton::clicked, this, [this] { m_backend.setDarkMode(true); });
  connect(lightRadio, &QRadioButton::clicked, this, [this] { m_backend.setDarkMode(false); });
  connect(&m_backend, &ClientBackend::darkModeChanged, &dialog, applyHintStyle);
  connect(interval, &QSpinBox::valueChanged, this,
          [this](int value) { m_backend.setIntervalMs(value); });
  connect(resetOrder, &QPushButton::clicked, this, [this] {
    m_model->resetSensorOrder();
    restoreExpandedState();
  });

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
  } else {
    m_tree->setColumnWidth(0, 220);
    m_tree->setColumnWidth(1, 100);
    m_tree->setColumnWidth(2, 100);
    m_tree->setColumnWidth(3, 100);
    m_tree->setColumnWidth(4, 100);
  }
  m_tree->header()->setStretchLastSection(false);
  m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
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
