#include "controller.h"
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMediaDevices>
#include <QtAudio>
#include <QTemporaryDir>
#include <QTimer>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#define REQUIRE(x)                                                             \
  do {                                                                         \
    if (!(x))                                                                  \
      throw std::runtime_error("Failed: " #x);                                 \
  } while (false)
static void waitFor(const std::function<bool()> &ready) {
  QElapsedTimer elapsed;
  elapsed.start();
  while (!ready()) {
    if (elapsed.elapsed() > 30000)
      throw std::runtime_error("Asynchronous operation timed out");
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  }
  QCoreApplication::processEvents();
}
static QByteArray read(const QString &p) {
  QFile f(p);
  REQUIRE(f.open(QIODevice::ReadOnly));
  return f.readAll();
}
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  try {
    REQUIRE(argc == 3 || argc == 4);
    QTemporaryDir temporary;
    REQUIRE(temporary.isValid());
    app.setOrganizationName("AyPlayerControllerTest");
    app.setApplicationName("AyPlayerControllerTest");
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope,
                       temporary.path());
    Controller controller(nullptr, false, temporary.filePath("session.json"));
    int loads = 0, exports = 0;
    QString lastExport;
    QObject::connect(&controller, &Controller::loaded, [&] { ++loads; });
    QObject::connect(&controller, &Controller::exported, [&](const QString &e) {
      ++exports;
      lastExport = e;
    });
    controller.openPath(QString::fromLocal8Bit(argv[1]));
    waitFor([&] { return loads == 1; });
    const auto reference = read(QString::fromLocal8Bit(argv[2])).mid(44);
    REQUIRE(controller.duration() == double(reference.size() / 4) / 48000);
    REQUIRE(controller.duration() > 25);
    REQUIRE(controller.error().isEmpty());
    REQUIRE(controller.playlist().size() == 1);
    controller.seek(13.25);
    REQUIRE(controller.position() == 13.25);
    REQUIRE(!controller.playing());
    controller.setVolume(.05);
    auto output = temporary.filePath("export.wav");
    controller.exportWav(QUrl::fromLocalFile(output));
    waitFor([&] { return exports == 1; });
    REQUIRE(lastExport.isEmpty());
    REQUIRE(controller.position() == 13.25);
    REQUIRE(read(output).mid(44) ==
            read(QString::fromLocal8Bit(argv[2])).mid(44));
    // Export cancellation leaves the playback cursor and completed output
    // intact.
    auto cancelled = temporary.filePath("cancelled.wav");
    controller.exportWav(QUrl::fromLocalFile(cancelled));
    controller.cancel();
    waitFor([&] { return exports == 2; });
    REQUIRE(lastExport == "Cancelled");
    REQUIRE(!QFile::exists(cancelled));
    REQUIRE(controller.position() == 13.25);
    controller.exportWav(QUrl::fromLocalFile(output));
    REQUIRE(controller.error().contains("already exists"));
    REQUIRE(exports == 3 && lastExport.contains("already exists"));
    REQUIRE(read(output).mid(44) == reference);
    controller.clearError();
    auto playlist = temporary.filePath("list.m3u8");
    controller.savePlaylist(QUrl::fromLocalFile(playlist));
    REQUIRE(read(playlist).startsWith("#EXTM3U\n"));
    controller.remove(0);
    REQUIRE(controller.playlist().isEmpty());
    controller.loadPlaylist(QUrl::fromLocalFile(playlist));
    waitFor([&] { return !controller.importing(); });
    REQUIRE(loads == 1);
    REQUIRE(controller.duration() == 0);
    REQUIRE(controller.playlist().size() == 1);
    controller.openPath(QString::fromLocal8Bit(argv[1]));
    waitFor([&] { return loads == 2; });
    auto batch = temporary.filePath("batch");
    REQUIRE(QDir().mkpath(batch));
    controller.seek(7);
    controller.exportPlaylistWav(QUrl::fromLocalFile(batch));
    waitFor([&] { return exports == 4; });
    REQUIRE(lastExport.isEmpty());
    REQUIRE(controller.position() == 7);
    REQUIRE(read(QDir(batch).filePath(QFileInfo(QString::fromLocal8Bit(argv[1])).completeBaseName()+".wav")).mid(44) ==
            read(output).mid(44));
    // Reopening the existing item loads it rather than silently doing nothing.
    controller.openPath(QString::fromLocal8Bit(argv[1]));
    waitFor([&] { return loads == 3; });
    REQUIRE(controller.playlist().size() == 1);
    // Import metadata only, in one model insertion, with no PCM load or
    // interruption of the currently prepared song. Thousands of native-picker
    // URLs should be processed without quadratic duplicate checks.
    controller.seek(9.5);
    QList<QUrl> many;
    auto bulk = temporary.filePath("bulk");
    REQUIRE(QDir().mkpath(bulk));
    for (int i = 0; i < 3000; ++i) {
      auto target = QDir(bulk).filePath(
          QString("track-%1.pt3").arg(i, 4, 10, QChar('0')));
      REQUIRE(QFile::copy(QString::fromLocal8Bit(argv[1]), target));
      many.append(QUrl::fromLocalFile(target));
    }
    int inserts = 0;
    QObject::connect(controller.playlistModel(),
                     &QAbstractItemModel::rowsInserted, [&] { ++inserts; });
    int heartbeats = 0;
    QTimer heartbeat;
    QObject::connect(&heartbeat, &QTimer::timeout, [&] { ++heartbeats; });
    heartbeat.start(1);
    QElapsedTimer importTime;
    importTime.start();
    controller.openFiles(many);
    REQUIRE(!controller.busy());
    waitFor([&] { return !controller.importing(); });
    heartbeat.stop();
    REQUIRE(loads == 3);
    REQUIRE(controller.position() == 9.5);
    REQUIRE(controller.playlist().size() == 3001);
    REQUIRE(inserts == 1);
    REQUIRE(heartbeats > 0);
    auto roles = controller.playlistModel()->roleNames();
    int titleRole = roles.key("trackTitle");
    int timeRole = roles.key("trackDuration");
    REQUIRE(controller.playlistModel()
                ->data(controller.playlistModel()->index(1, 0), titleRole)
                .toString() == QString::fromLatin1(read(QString::fromLocal8Bit(argv[1])).mid(30,32)).trimmed().split(QChar(0)).first());
    REQUIRE(controller.playlistModel()
                ->data(controller.playlistModel()->index(1, 0), timeRole)
                .toDouble() > 51);
    controller.select(2000);
    REQUIRE(!controller.busy());
    REQUIRE(loads == 3);
    controller.move(2000, 100);
    REQUIRE(controller.selected() == 100);
    controller.remove(100);
    REQUIRE(controller.playlist().size() == 3000);
    controller.openFiles(many);
    waitFor([&] { return !controller.importing(); });
    REQUIRE(controller.playlist().size() == 3001);
    std::cout << "3000-file import: " << importTime.elapsed() << " ms, "
              << heartbeats << " GUI heartbeats, no PCM loads\n";
    controller.clearPlaylist();
    REQUIRE(controller.playlistModel()->rowCount() == 0);
    if (argc == 4) {
      QString folder = QString::fromLocal8Bit(argv[3]);
      int expected = 0;
      QDirIterator it(folder,
                      {"*.pt3", "*.PT3", "*.psg", "*.PSG", "*.ym", "*.YM"},
                      QDir::Files, QDirIterator::Subdirectories);
      while (it.hasNext()) {
        it.next();
        ++expected;
      }
      importTime.restart();
      controller.openFolder(QUrl::fromLocalFile(folder));
      waitFor([&] { return !controller.importing(); });
      REQUIRE(controller.playlistModel()->rowCount() == expected);
      REQUIRE(loads == 3);
      REQUIRE(controller.duration() == 0);
      std::cout << "Recursive music import: " << expected << " files in "
                << importTime.elapsed() << " ms, no PCM loads\n";
      controller.clearPlaylist();
    }
    // Missing entries must end their asynchronous load; next cannot spin
    // forever.
    controller.openPath(temporary.filePath("missing1.pt3"));
    waitFor([&] { return !controller.busy() && !controller.importing(); });
    REQUIRE(!controller.error().isEmpty());
    controller.openPath(temporary.filePath("missing2.pt3"));
    waitFor([&] { return !controller.busy() && !controller.importing(); });
    controller.next();
    waitFor([&] { return !controller.busy() && !controller.importing(); });
    REQUIRE(!controller.playing());
    // A later request supersedes an in-flight open without requiring a wait
    // or showing errors for the canceled intermediate track.
    Controller rapid(nullptr, false, temporary.filePath("rapid.json"));
    int rapidLoads = 0;
    QObject::connect(&rapid, &Controller::loaded, [&] { ++rapidLoads; });
    rapid.openPath(QString::fromLocal8Bit(argv[1]));
    waitFor([&] { return rapidLoads == 1; });
    rapid.openPath(QString::fromLocal8Bit(argv[1]));
    rapid.openPath(temporary.filePath("missing2.pt3"));
    rapid.openPath(QString::fromLocal8Bit(argv[1]));
    waitFor([&] { return rapidLoads == 2; });
    REQUIRE(rapid.error().isEmpty());
    REQUIRE(!rapid.busy());
    if (!QMediaDevices::defaultAudioOutput().isNull()) {
      // Check the actual sink on creation and on updates. Suspend it before
      // exercising endpoints so the test does not emit full-volume audio.
      rapid.setVolume(.5);
      rapid.play();
      waitFor([&] { return rapid.playing(); });
      auto *sink = rapid.findChild<QAudioSink *>();
      REQUIRE(sink);
      const auto midpointGain = sink->volume();
      REQUIRE(midpointGain > 0 && midpointGain < .3);
      rapid.pause();
      rapid.setVolume(.25);
      REQUIRE(sink->volume() > 0 && sink->volume() < midpointGain);
      rapid.setVolume(.75);
      REQUIRE(sink->volume() > midpointGain && sink->volume() < .75);
      rapid.setVolume(-1); REQUIRE(rapid.volume() == 0 && sink->volume() == 0);
      rapid.setVolume(2); REQUIRE(rapid.volume() == 1 && sink->volume() == 1);
      rapid.setVolume(0);
    }
    const auto legacyPath = temporary.filePath("legacy-volume.json");
    {
      QFile legacy(legacyPath); REQUIRE(legacy.open(QIODevice::WriteOnly));
      legacy.write(QJsonDocument(QJsonObject{{"version",1},{"volume",.25},
          {"playlist",QJsonArray{}}}).toJson());
    }
    {
      Controller migrated(nullptr,false,legacyPath);
      REQUIRE(migrated.volume() > .25 && migrated.volume() < 1);
      const auto gain = QtAudio::convertVolume(float(migrated.volume()),
          QtAudio::LogarithmicVolumeScale,QtAudio::LinearVolumeScale);
      REQUIRE(std::abs(gain-.25) < .0001);
      migrated.setVolume(.42);
    }
    Controller restoredVolume(nullptr,false,legacyPath);
    REQUIRE(restoredVolume.volume() == .42);
    QFile volumeFile(legacyPath); REQUIRE(volumeFile.open(QIODevice::ReadOnly));
    REQUIRE(QJsonDocument::fromJson(volumeFile.readAll()).object()
        .value("volumeScale").toString() == "logarithmic");
    std::cout << "Qt controller: async open, seek, export independence, volume "
                 "isolation, cancellation, no overwrite, M3U roundtrip, batch "
                 "and missing files passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
