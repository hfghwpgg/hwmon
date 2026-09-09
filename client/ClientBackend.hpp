#pragma once

#include "MonitorModel.hpp"

#include <QJsonObject>
#include <QObject>
#include <QString>

class QLocalSocket;
class QTimer;

class ClientBackend : public QObject {
  Q_OBJECT

public:
  ClientBackend(QString socketPath, QString dataFile, QObject *parent = nullptr);

  MonitorModel *model() const {
    return m_model;
  }
  bool connected() const {
    return m_connected;
  }
  bool fileMode() const {
    return m_fileMode;
  }
  QString statusText() const {
    return m_statusText;
  }
  QString elapsedText() const {
    return m_elapsedText;
  }
  bool darkMode() const {
    return m_darkMode;
  }
  void setDarkMode(bool dark);
  int intervalMs() const {
    return m_intervalMs;
  }
  void setIntervalMs(int ms);

  void start();
  void resetReadings();

signals:
  void connectedChanged();
  void statusTextChanged();
  void elapsedTextChanged();
  void darkModeChanged();
  void intervalMsChanged();

private:
  void connectToServer();
  void sendCommand(const QString &cmd);
  void sendJson(const QJsonObject &object);
  void sendIntervalToServer();
  void applyAppPalette();
  void handleReadyRead();
  void handleDisconnected();
  void poll();
  void updateElapsed();
  void applyPayload(const QByteArray &payload);
  void loadFile();
  void setConnected(bool connected);
  void setStatus(const QString &text);

  MonitorModel *m_model = nullptr;
  QLocalSocket *m_socket = nullptr;
  QTimer *m_pollTimer = nullptr;
  QTimer *m_reconnectTimer = nullptr;
  QTimer *m_clockTimer = nullptr;
  QByteArray m_buffer;
  QString m_socketPath;
  QString m_dataFile;
  QString m_statusText;
  QString m_elapsedText{QStringLiteral("00:00:00")};
  qint64 m_startTimestamp = 0;
  bool m_fileMode = false;
  bool m_connected = false;
  bool m_darkMode = true;
  bool m_awaitingReply = false;
  int m_intervalMs = 1000;
};
