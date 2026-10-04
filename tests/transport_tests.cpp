#include "controller.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMediaDevices>
#include <QTemporaryDir>
#include <functional>
#include <iostream>
#include <stdexcept>
#define REQUIRE(x) do { if (!(x)) throw std::runtime_error("Failed: " #x); } while(false)
static void waitFor(const std::function<bool()> &ready) {
  QElapsedTimer t; t.start();
  while (!ready()) {
    if (t.elapsed()>10000) throw std::runtime_error("Transport timed out");
    QCoreApplication::processEvents(QEventLoop::AllEvents,10);
  }
}
int main(int argc,char **argv) {
  QCoreApplication app(argc,argv);
  if (QMediaDevices::defaultAudioOutput().isNull()) {
    std::cout << "NOT_EXECUTED: no audio device for EOF/transport test\n"; return 77;
  }
  try {
    REQUIRE(argc==3); QTemporaryDir work; REQUIRE(work.isValid());
    app.setOrganizationName("CppayTransportTests");app.setApplicationName("CppayTransportTests");
    QSettings::setPath(QSettings::NativeFormat,QSettings::UserScope,work.path());
    Controller player(nullptr,false,work.filePath("session.json"));player.setVolume(0);
    int loads=0;QObject::connect(&player,&Controller::loaded,[&]{++loads;});
    player.openPath(QString::fromLocal8Bit(argv[1]));waitFor([&]{return loads==1;});
    player.openFiles({QUrl::fromLocalFile(QString::fromLocal8Bit(argv[2]))});
    waitFor([&]{return !player.importing();});REQUIRE(player.playlist().size()==2);
    player.seek(player.duration()-.1);player.play();waitFor([&]{return loads==2;});
    REQUIRE(player.currentTrack()==1);waitFor([&]{return !player.playing();});
    REQUIRE(loads==2);REQUIRE(player.position()==player.duration());
    // Restart and pause/resume must use the current track, not advance twice.
    player.stop();REQUIRE(player.position()==0);player.play();waitFor([&]{return player.position()>.03;});
    player.pause();REQUIRE(!player.playing());const auto paused=player.position();
    player.pause();waitFor([&]{return player.position()>paused+.03;});
    player.stop();REQUIRE(!player.playing() && player.position()==0);
    // Whole-song repeat crosses the natural end without a new loader job.
    player.setLoop(true);player.seek(player.duration()-.1);player.play();
    waitFor([&]{return player.position()<.3 && player.playing();});
    REQUIRE(loads==2 && player.currentTrack()==1);player.stop();
    REQUIRE(player.error().isEmpty());
    std::cout << "PASS: actual device EOF advancement once, final EOF, pause/resume, stop, whole-song repeat\n";
    return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
