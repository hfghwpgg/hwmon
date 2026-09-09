#pragma once

#include <QMainWindow>

class ClientBackend;
class MonitorModel;
class QLabel;
class QPushButton;
class QTreeView;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(ClientBackend &backend, QWidget *parent = nullptr);

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  void applyTheme();
  void restoreExpandedState();
  void updateFooter();
  void showContextMenu(const QPoint &pos);
  void openSettings();
  void saveHeaderState() const;
  void restoreHeaderState();
  void saveWindowState() const;
  void restoreWindowState();

  ClientBackend &m_backend;
  MonitorModel *m_model = nullptr;
  QTreeView *m_tree = nullptr;
  QLabel *m_statusLabel = nullptr;
  QLabel *m_elapsedLabel = nullptr;
  QPushButton *m_resetButton = nullptr;
  QWidget *m_footer = nullptr;
};
