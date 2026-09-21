#include "ClientBackend.hpp"
#include "MainWindow.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QStyleFactory>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("hwmon"));
  QCoreApplication::setApplicationName(QStringLiteral("hwmon-client"));
  QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("hwmon Qt client"));
  parser.addHelpOption();

  QCommandLineOption socketOption({QStringLiteral("s"), QStringLiteral("socket")},
                                  QStringLiteral("Unix domain socket path"), QStringLiteral("path"),
                                  QStringLiteral("/tmp/hwmon/hwmon.sock"));
  QCommandLineOption dataFileOption(
      {QStringLiteral("f"), QStringLiteral("data-file")},
      QStringLiteral("Load a snapshot from a JSON file instead of the server"),
      QStringLiteral("path"));
  parser.addOption(socketOption);
  parser.addOption(dataFileOption);
  parser.process(app);

  ClientBackend backend(parser.value(socketOption), parser.value(dataFileOption));
  MainWindow window(backend);
  window.show();
  backend.start();

  return QApplication::exec();
}
