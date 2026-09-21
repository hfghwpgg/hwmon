#include "ClientBackend.hpp"

#include "MonitorModel.hpp"
#include "UiStyle.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocalSocket>
#include <QPalette>
#include <QSettings>
#include <QTimer>
#include <utility>

ClientBackend::ClientBackend(QString socketPath, QString dataFile, QObject *parent) :
    QObject(parent),
    m_model(new MonitorModel(this)),
    m_socket(new QLocalSocket(this)),
    m_pollTimer(new QTimer(this)),
    m_reconnectTimer(new QTimer(this)),
    m_clockTimer(new QTimer(this)),
    m_socketPath(std::move(socketPath)),
    m_dataFile(std::move(dataFile)),
    m_fileMode(!m_dataFile.isEmpty()) {
  QSettings settings;
  m_darkMode = settings.value(QStringLiteral("darkMode"), true).toBool();
  m_intervalMs = qBound(50, settings.value(QStringLiteral("intervalMs"), 1000).toInt(), 60000);
  m_statusText = m_fileMode ? QStringLiteral("File mode") : QStringLiteral("Disconnected");

  m_model->setDarkTheme(m_darkMode);
  m_model->setFlashDurationMs(m_intervalMs);
  applyAppPalette();
  m_pollTimer->setInterval(m_intervalMs);
  m_reconnectTimer->setInterval(2000);
  m_clockTimer->setInterval(250);

  connect(m_socket, &QLocalSocket::connected, this, [this] {
    m_buffer.clear();
    m_awaitingReply = false;
    setConnected(true);
    setStatus(QStringLiteral("Connected"));
    sendIntervalToServer();
    m_pollTimer->start();
    poll();
  });
  connect(m_socket, &QLocalSocket::readyRead, this, &ClientBackend::handleReadyRead);
  connect(m_socket, &QLocalSocket::disconnected, this, &ClientBackend::handleDisconnected);
  connect(m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
    if (m_socket->state() != QLocalSocket::ConnectedState) {
      handleDisconnected();
    }
  });
  connect(m_pollTimer, &QTimer::timeout, this, &ClientBackend::poll);
  connect(m_reconnectTimer, &QTimer::timeout, this, [this] {
    if (!m_fileMode && m_socket->state() == QLocalSocket::UnconnectedState) {
      connectToServer();
    }
  });
  connect(m_clockTimer, &QTimer::timeout, this, &ClientBackend::updateElapsed);
}

void ClientBackend::setDarkMode(bool dark) {
  if (m_darkMode == dark) {
    return;
  }
  m_darkMode = dark;
  QSettings settings;
  settings.setValue(QStringLiteral("darkMode"), m_darkMode);
  m_model->setDarkTheme(m_darkMode);
  applyAppPalette();
  emit darkModeChanged();
}

void ClientBackend::setIntervalMs(int ms) {
  ms = qBound(50, ms, 60000);
  if (m_intervalMs == ms) {
    return;
  }
  m_intervalMs = ms;
  m_pollTimer->setInterval(m_intervalMs);
  m_model->setFlashDurationMs(m_intervalMs);
  QSettings settings;
  settings.setValue(QStringLiteral("intervalMs"), m_intervalMs);
  emit intervalMsChanged();
  sendIntervalToServer();
}

void ClientBackend::start() {
  m_clockTimer->start();
  if (m_fileMode) {
    loadFile();
    updateElapsed();
    return;
  }
  connectToServer();
  m_reconnectTimer->start();
}

void ClientBackend::resetReadings() {
  if (m_fileMode) {
    m_model->resetLocalReadings();
    m_startTimestamp = QDateTime::currentSecsSinceEpoch();
    updateElapsed();
    return;
  }
  if (!m_connected) {
    return;
  }
  sendCommand(QStringLiteral("reset"));
}

void ClientBackend::connectToServer() {
  if (m_socket->state() != QLocalSocket::UnconnectedState) {
    return;
  }
  m_socket->connectToServer(m_socketPath);
}

void ClientBackend::sendCommand(const QString &cmd) {
  sendJson(QJsonObject{{QStringLiteral("cmd"), cmd}});
}

void ClientBackend::sendJson(const QJsonObject &object) {
  if (m_socket->state() != QLocalSocket::ConnectedState) {
    return;
  }
  QByteArray line = QJsonDocument(object).toJson(QJsonDocument::Compact);
  line.append('\n');
  m_socket->write(line);
  m_socket->flush();
}

void ClientBackend::sendIntervalToServer() {
  sendJson(QJsonObject{
      {QStringLiteral("cmd"), QStringLiteral("set_interval")},
      {QStringLiteral("value"), m_intervalMs},
  });
}

void ClientBackend::applyAppPalette() {
  const ThemeColors &theme = themeColors(m_darkMode);
  QPalette palette;
  palette.setColor(QPalette::Window, theme.window);
  palette.setColor(QPalette::WindowText, theme.text);
  palette.setColor(QPalette::Base, theme.base);
  palette.setColor(QPalette::AlternateBase, theme.alternateBase);
  palette.setColor(QPalette::Text, theme.text);
  palette.setColor(QPalette::Button, theme.button);
  palette.setColor(QPalette::ButtonText, theme.text);
  palette.setColor(QPalette::Light, theme.light);
  palette.setColor(QPalette::Midlight, theme.midlight);
  palette.setColor(QPalette::Mid, theme.mid);
  palette.setColor(QPalette::Dark, theme.dark);
  palette.setColor(QPalette::Shadow, theme.shadow);
  palette.setColor(QPalette::Highlight, theme.accent);
  palette.setColor(QPalette::HighlightedText, theme.highlightedText);
  palette.setColor(QPalette::ToolTipBase, theme.tooltipBase);
  palette.setColor(QPalette::ToolTipText, theme.text);
  palette.setColor(QPalette::PlaceholderText, theme.mutedText);
  if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance())) {
    app->setPalette(palette);
  }
}

void ClientBackend::handleReadyRead() {
  m_buffer.append(m_socket->readAll());
  while (true) {
    const qsizetype newline = m_buffer.indexOf('\n');
    if (newline < 0) {
      break;
    }
    const QByteArray line = m_buffer.left(newline).trimmed();
    m_buffer.remove(0, newline + 1);
    m_awaitingReply = false;
    if (!line.isEmpty()) {
      applyPayload(line);
    }
  }
}

void ClientBackend::handleDisconnected() {
  m_pollTimer->stop();
  m_awaitingReply = false;
  m_buffer.clear();
  if (m_socket->state() != QLocalSocket::UnconnectedState) {
    m_socket->abort();
  }
  setConnected(false);
  setStatus(QStringLiteral("Disconnected"));
}

void ClientBackend::poll() {
  if (m_awaitingReply || m_socket->state() != QLocalSocket::ConnectedState) {
    return;
  }
  m_awaitingReply = true;
  sendCommand(QStringLiteral("get"));
}

void ClientBackend::updateElapsed() {
  if (!m_fileMode && !m_connected) {
    return;
  }
  qint64 elapsed = 0;
  if (m_startTimestamp > 0) {
    elapsed = QDateTime::currentSecsSinceEpoch() - m_startTimestamp;
    if (elapsed < 0) {
      elapsed = 0;
    }
  }
  const int hours = static_cast<int>(elapsed / 3600);
  const int minutes = static_cast<int>((elapsed % 3600) / 60);
  const int seconds = static_cast<int>(elapsed % 60);
  const QString text = QStringLiteral("%1:%2:%3")
                           .arg(hours, 2, 10, QLatin1Char('0'))
                           .arg(minutes, 2, 10, QLatin1Char('0'))
                           .arg(seconds, 2, 10, QLatin1Char('0'));
  if (text != m_elapsedText) {
    m_elapsedText = text;
    emit elapsedTextChanged();
  }
}

void ClientBackend::applyPayload(const QByteArray &payload) {
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
  if (error.error != QJsonParseError::NoError) {
    return;
  }

  if (document.isObject()) {
    const QJsonObject object = document.object();
    if (object.contains(QLatin1String("error"))) {
      setStatus(QStringLiteral("Server error"));
    }
    return;
  }

  qint64 timestamp = 0;
  if (m_model->applySnapshot(document, &timestamp)) {
    if (timestamp > 0) {
      m_startTimestamp = timestamp;
    }
    if (m_connected) {
      setStatus(QStringLiteral("Connected"));
    }
    updateElapsed();
  }
}

void ClientBackend::loadFile() {
  QFile file(m_dataFile);
  if (!file.open(QIODevice::ReadOnly)) {
    setStatus(QStringLiteral("Failed to read file"));
    return;
  }

  qint64 timestamp = 0;
  if (m_model->applySnapshot(QJsonDocument::fromJson(file.readAll()), &timestamp)) {
    m_startTimestamp = timestamp;
    setStatus(QStringLiteral("File mode"));
  } else {
    setStatus(QStringLiteral("Invalid data file"));
  }
}

void ClientBackend::setConnected(bool connected) {
  if (m_connected == connected) {
    return;
  }
  m_connected = connected;
  emit connectedChanged();
}

void ClientBackend::setStatus(const QString &text) {
  if (m_statusText == text) {
    return;
  }
  m_statusText = text;
  emit statusTextChanged();
}
