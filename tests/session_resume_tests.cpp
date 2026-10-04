#include "controller.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QTemporaryDir>
#include <QThread>
#include <functional>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do { if (!(x)) throw std::runtime_error("Failed: " #x); } while(false)
static void waitFor(const std::function<bool()> &predicate) {
  QElapsedTimer timer; timer.start();
  while (!predicate()) {
    CHECK(timer.elapsed() < 45000);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    QThread::msleep(2);
  }
}
static QJsonObject snapshot(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  return QJsonDocument::fromJson(file.readAll()).object();
}
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  app.setOrganizationName("AyPlayerSessionTest");
  app.setApplicationName("AyPlayerSessionTest");
  QSettings::setDefaultFormat(QSettings::IniFormat);
  try {
    const auto args = app.arguments();
    if (args.size() > 2 && args[1] == "--writer") {
      QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, args[2]+"-settings");
      std::cerr << "Writer: importing\n";
      Controller player(nullptr, false, args[2]);
      bool loaded = false;
      QObject::connect(&player, &Controller::loaded, [&] { loaded = true; });
      player.openFiles({QUrl::fromLocalFile(args[3]), QUrl::fromLocalFile(args[4])});
      waitFor([&] { return !player.importing(); });
      player.openPath(args[4]);
      waitFor([&] { return loaded; });
      player.seek(12.25);
      player.setVolume(.23);
      player.setLoop(true);
      player.move(1, 0);
      player.select(1);
      // The move saves 12.25 immediately; change the cursor afterwards so only
      // the real ten-second timer can produce the checkpoint we wait for.
      player.seek(17.75);
      QFile ready(args[2]+".ready");
      CHECK(ready.open(QIODevice::WriteOnly));
      CHECK(ready.write("ready") == 5); ready.close();
      std::cerr << "Writer: awaiting ten-second checkpoint\n";
      return app.exec();
    }
    if (args.size() > 2 && args[1] == "--reader") {
      QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, args[2]+"-settings");
      {
        Controller player(nullptr, true, args[2]);
        bool loaded = false;
        QObject::connect(&player, &Controller::loaded, [&] { loaded = true; });
        waitFor([&] { return loaded; });
        CHECK(player.playlist().size() == 2);
        CHECK(player.currentTrack() == 0);
        CHECK(player.selected() == 1);
        CHECK(player.playlist()[0] == args[3]);
        CHECK(player.position() == args[4].toDouble());
        CHECK(!player.playing());
        CHECK(player.loop()); CHECK(player.volume() == .23);
        CHECK(player.ym() && player.preamp() == 127 && player.clock() == 1773400);
        player.seek(24.5); // destructor must save immediately, before ten seconds.
      }
      return 0;
    }
    if (args.size() > 2 && (args[1] == "--playing-writer" || args[1] == "--playing-reader")) {
      QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, args[2]+"-settings");
      Controller player(nullptr, true, args[2]);
      bool loaded = false;
      QObject::connect(&player, &Controller::loaded, [&] { loaded = true; });
      waitFor([&] { return loaded; });
      CHECK(player.currentTrack() == 0 && player.selected() == 1);
      if (args[1] == "--playing-writer") {
        player.pause(); // resume the active song, even with another row selected
        waitFor([&] { return player.position() > 25; });
      } else {
        waitFor([&] { return player.playing(); });
        CHECK(player.position() >= args[3].toDouble());
      }
      CHECK(player.currentTrack() == 0 && player.selected() == 1);
      return 0; // preserve the playing state on a normal close
    }
    CHECK(args.size() == 2);
    QTemporaryDir temporary; CHECK(temporary.isValid());
    const QString first = QFileInfo(args[1]).absoluteFilePath();
    const QString second = temporary.filePath("second song Ω.pt3");
    CHECK(QFile::copy(first, second));
    const QString session = temporary.filePath("session.json");
    QProcess writer;
    writer.setProcessChannelMode(QProcess::ForwardedChannels);
    writer.start(app.applicationFilePath(), {"--writer", session, first, second});
    CHECK(writer.waitForStarted());
    waitFor([&] { CHECK(writer.state() != QProcess::NotRunning); return QFile::exists(session+".ready"); });
    QElapsedTimer elapsed; elapsed.start();
    waitFor([&] { CHECK(writer.state() != QProcess::NotRunning); return snapshot(session).value("position").toDouble() == 17.75; });
    CHECK(elapsed.elapsed() < 11000);
    writer.kill(); CHECK(writer.waitForFinished()); // no destructor or aboutToQuit
    CHECK(writer.exitStatus() == QProcess::CrashExit);
    auto saved = snapshot(session);
    CHECK(saved.value("activeTrack").toString() == second);
    CHECK(saved.value("selectedTrack").toString() == first);
    CHECK(saved.value("position").toDouble() == 17.75);
    auto reader = [&](const QString &position) {
      QProcess process;
      process.start(app.applicationFilePath(), {"--reader", session, second, position});
      CHECK(process.waitForFinished(20000));
      if (process.exitCode() != 0) std::cerr << process.readAllStandardError().constData();
      CHECK(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0);
    };
    reader("17.75");
    CHECK(snapshot(session).value("position").toDouble() == 24.5);
    reader("24.5");
    if (!QMediaDevices::defaultAudioOutput().isNull()) {
      auto run = [&](const QStringList &arguments) {
        QProcess process; process.start(app.applicationFilePath(), arguments);
        CHECK(process.waitForFinished(20000));
        if (process.exitCode() != 0) std::cerr << process.readAllStandardError().constData();
        CHECK(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0);
      };
      run({"--playing-writer", session});
      auto playing = snapshot(session);
      CHECK(playing.value("playing").toBool());
      CHECK(playing.value("position").toDouble() > 25);
      run({"--playing-reader", session, QString::number(playing.value("position").toDouble(), 'g', 17)});
      std::cout << "PASS: audible playback and automatic resume with another row selected\n";
    }
    std::cout << "PASS: ten-second checkpoint, forced kill, ordered playlist, separate selection, cursor, paused state, settings, immediate clean-close checkpoint\n";
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
