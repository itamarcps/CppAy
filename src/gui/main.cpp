#include "controller.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QWidget>
#include <cmath>
int main(int argc, char **argv) {
  // A Widgets application gives Qt Quick access to native desktop dialogs.
#ifndef Q_OS_WIN
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME") &&
      qEnvironmentVariable("XDG_CURRENT_DESKTOP")
          .contains("KDE", Qt::CaseInsensitive))
    qputenv("QT_QPA_PLATFORMTHEME", "kde");
#endif
#ifndef AYPLAYER_MEMORY_BASELINE
  // This player draws a small 2D interface and synthesizes raw PCM. Avoid
  // allocating a GPU scene graph or probing unrelated video codec devices.
  if (!qEnvironmentVariableIsSet("QT_QUICK_BACKEND"))
    QQuickWindow::setSceneGraphBackend(QStringLiteral("software"));
  if (!qEnvironmentVariableIsSet("QT_FFMPEG_DECODING_HW_DEVICE_TYPES"))
    qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", ",");
  if (!qEnvironmentVariableIsSet("QT_FFMPEG_ENCODING_HW_DEVICE_TYPES"))
    qputenv("QT_FFMPEG_ENCODING_HW_DEVICE_TYPES", ",");
#endif
  QApplication app(argc, argv);
  app.setApplicationVersion(CPPAY_VERSION);
  auto args = app.arguments();
  if (args.contains("--version")) {
    fprintf(stdout, "C++Ay %s\n", CPPAY_VERSION);
    return 0;
  }
  bool memorySmoke = args.contains("--memory-smoke-test");
  bool windowSmoke = args.contains("--window-smoke-test");
  bool layoutSmoke = args.contains("--layout-smoke-test");
  bool streamSmoke = args.contains("--stream-smoke-test");
  bool playlistSmoke = args.contains("--playlist-smoke-test");
  bool dialogSmoke = args.contains("--dialog-smoke-test");
  bool smoke = args.contains("--smoke-test") || playlistSmoke || dialogSmoke ||
               streamSmoke || layoutSmoke || memorySmoke || windowSmoke;
  if (smoke && !QDir().mkpath("evidence")) {
    fprintf(stderr, "Cannot create smoke-test evidence directory\n");
    return 1;
  }
  app.setOrganizationName(smoke ? "AyPlayerTests" : "AyPlayer");
  // Keep the existing storage IDs so the rename preserves sessions/settings.
  app.setApplicationName("AyPlayer");
  app.setApplicationDisplayName("C++Ay");
  app.setDesktopFileName("cppay");
  QQuickStyle::setStyle("Basic");
  app.setWindowIcon(QIcon(":/assets/cppay-icon.png"));
  QStringList paths;
  for (int i = 1; i < args.size(); ++i)
    if (!args[i].startsWith('-'))
      paths.append(QFileInfo(args[i]).absoluteFilePath());
  QLocalServer server;
  if (!smoke) {
    const QString name =
        "AyPlayer-" + qEnvironmentVariable(
                          "USER", qEnvironmentVariable("USERNAME", "desktop"));
    QLocalSocket socket;
    socket.connectToServer(name);
    if (socket.waitForConnected(200)) {
      QJsonArray files;
      for (const auto &p : paths)
        files.append(p);
      socket.write(QJsonDocument(files).toJson(QJsonDocument::Compact));
      socket.flush();
      socket.waitForBytesWritten(500);
      return 0;
    }
    server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server.listen(name) &&
        socket.error() == QLocalSocket::ConnectionRefusedError) {
      QLocalServer::removeServer(name);
      server.listen(name);
    }
  }
  Controller player(nullptr, !smoke && paths.isEmpty());
  if (smoke)
    player.clearPlaylist();
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("player", &player);
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
  engine.loadFromModule("AyPlayer", "Main");
  if (engine.rootObjects().isEmpty())
    return 1;
  auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
  QSettings settings;
  if (!smoke) {
    window->resize(settings.value("windowWidth", 1120).toInt(),
                   settings.value("windowHeight", 740).toInt());
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&] {
      settings.setValue("windowWidth", window->width());
      settings.setValue("windowHeight", window->height());
    });
  }
  QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
    while (auto *client = server.nextPendingConnection()) {
      QObject::connect(
          client, &QLocalSocket::readyRead, &player, [client, &player, window] {
            auto buffer =
                client->property("request").toByteArray() + client->readAll();
            client->setProperty("request", buffer);
            QJsonParseError error;
            auto doc = QJsonDocument::fromJson(buffer, &error);
            if (error.error != QJsonParseError::NoError)
              return;
            QList<QUrl> files;
            for (auto p : doc.array())
              files.append(QUrl::fromLocalFile(p.toString()));
            player.openFiles(files);
            window->show();
            window->raise();
            window->requestActivate();
            client->disconnectFromServer();
          });
      QObject::connect(client, &QLocalSocket::disconnected, client,
                       &QObject::deleteLater);
    }
  });
  if (layoutSmoke) {
    auto step = std::make_shared<int>(0);
    auto checks = std::make_shared<QJsonArray>();
    auto failures = std::make_shared<QJsonArray>();
    auto run = std::make_shared<std::function<void()>>();
    const QList<QSize> sizes{{760,520},{820,600},{1120,740},{1280,800}};
    std::weak_ptr<std::function<void()>> weakRun = run;
    *run = [&, step, checks, failures, weakRun, sizes] {
      auto run = weakRun.lock();
      if (!run) return;
      int current = *step;
      if (current == sizes.size()) {
        if (paths.size() > 1) {
          ++*step;
          player.openPath(paths[1]);
          return;
        }
        *step = sizes.size()*2+1;
        current = *step;
      }
      if (current >= sizes.size()*2+1) {
        QFile report("evidence/ui-layout.json");
        if (report.open(QIODevice::WriteOnly))
          report.write(QJsonDocument(QJsonObject{{"cases", *checks}, {"failures", *failures},
                        {"status", failures->isEmpty() ? "PASS" : "FAIL"}}).toJson());
        app.exit(failures->isEmpty() ? 0 : 1);
        return;
      }
      int sizeIndex = current < sizes.size() ? current : current-sizes.size()-1;
      if (sizeIndex < 0) sizeIndex = 0;
      window->resize(sizes[sizeIndex]);
      QTimer::singleShot(200, &app, [&, step, checks, failures, run, sizes, sizeIndex] {
        QJsonArray bounds;
        const QRectF screen(0,0,window->width(),window->height());
        auto rect = [](QQuickItem *item) { return item->mapRectToScene(QRectF(0,0,item->width(),item->height())); };
        auto *deck = window->findChild<QQuickItem *>("playerDeck");
        auto *playlist = window->findChild<QQuickItem *>("playlistPanel");
        for (const auto &name : {"windowTitleBar", "windowMenuBar", "windowDragArea", "windowMinimizeButton", "windowMaximizeButton", "windowCloseButton", "positionDisplay", "scopeGrid", "timeline", "volumeControls", "transportControls", "playlistView", "playlistToolsBar", "statusStrip"}) {
          auto *item = window->findChild<QQuickItem *>(name);
          if (!item) { failures->append(QString("Missing item: ")+name); continue; }
          auto box = rect(item);
          bool inside = screen.adjusted(-1,-1,1,1).contains(box);
          if (QString(name)=="transportControls" || QString(name)=="volumeControls")
            inside &= deck && rect(deck).contains(box);
          if (QString(name)=="playlistToolsBar" || QString(name)=="playlistView")
            inside &= playlist && rect(playlist).contains(box);
          inside &= item->width()>0 && item->height()>0;
          if (!inside) failures->append(QString("Clipped control: ")+name+QString(" at %1x%2").arg(window->width()).arg(window->height()));
          bounds.append(QJsonObject{{"item",name},{"x",box.x()},{"y",box.y()},{"width",box.width()},{"height",box.height()},{"inside",inside}});
        }
        std::function<QQuickItem *(QQuickItem *, const QString &)> findVisualItem;
        findVisualItem = [&findVisualItem](QQuickItem *root, const QString &name) -> QQuickItem * {
          if (root->objectName()==name) return root;
          for (auto *child : root->childItems())
            if (auto *match = findVisualItem(child,name)) return match;
          return nullptr;
        };
        auto *grid = findVisualItem(window->contentItem(), "scopeGrid");
        for (int i=0; i<player.voiceCount(); ++i) {
          auto *scope = findVisualItem(window->contentItem(), QString("channelScope%1").arg(i));
          if (!scope || !grid || scope->height()<8 || !rect(grid).contains(rect(scope)))
            failures->append(QString("Clipped channel scope %1 at %2x%3").arg(i).arg(window->width()).arg(window->height()));
        }
        const QString image = QString("evidence/retro-%1ch-%2x%3.png").arg(player.voiceCount()).arg(window->width()).arg(window->height());
        window->grabWindow().save(image);
        checks->append(QJsonObject{{"width",window->width()},{"height",window->height()},{"voices",player.voiceCount()},{"controls",bounds},{"image",image}});
        ++*step;
        // The second track starts at step 5; include all four sizes for both chips.
        (*run)();
      });
    };
    QObject::connect(&player, &Controller::loaded, &app, [&,run] {
      auto folder=qEnvironmentVariable("AYPLAYER_LAYOUT_IMPORT");
      if(!folder.isEmpty() && player.playlistModel()->rowCount()<10)
        player.openFolder(QUrl::fromLocalFile(folder));
      QTimer::singleShot(600,&app,[run]{(*run)();});
    });
    QTimer::singleShot(20000,&app,[]{QCoreApplication::exit(3);});
  }
  bool audioStarted = false, positionAdvanced = false,
       mouseSeekVerified = false, importWhilePlaying = false, mixerVerified = false, listToolsVerified = false;
  if (windowSmoke) {
    app.setQuitOnLastWindowClosed(false);
    auto failures = std::make_shared<QJsonArray>();
    auto step = std::make_shared<int>(0);
    auto clock = std::make_shared<ulong>(1000);
    auto minimized = std::make_shared<bool>(false);
    QObject::connect(window,&QWindow::windowStateChanged,&app,[minimized](Qt::WindowState state) {
      if (state==Qt::WindowMinimized) *minimized=true;
    });
    auto timer = new QTimer(&app);
    timer->setInterval(250);
    const QSize initialSize = window->size();
    auto click = [window,clock](QQuickItem *item, bool twice=false) {
      if (!item) return;
      auto point = item->mapToScene(QPointF(item->width()/2,item->height()/2));
      auto global = window->mapToGlobal(point.toPoint());
      for (int i=0; i<(twice?2:1); ++i) {
        QMouseEvent press(QEvent::MouseButtonPress,
                         point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        press.setTimestamp(*clock); release.setTimestamp(*clock+20); *clock+=60;
        QCoreApplication::sendEvent(window,&press); QCoreApplication::sendEvent(window,&release);
      }
    };
    QObject::connect(timer,&QTimer::timeout,&app,[&,timer,step,failures,click,initialSize,minimized] {
      auto *maximize = window->findChild<QQuickItem *>("windowMaximizeButton");
      auto *frame = window->findChild<QQuickItem *>("windowResizeHandles");
      auto check = [&](bool condition, const QString &message) { if (!condition) failures->append(message); };
      switch ((*step)++) {
      case 0: {
        check(window->flags().testFlag(Qt::FramelessWindowHint),"Native title bar still enabled");
        check(frame && frame->isVisible(),"Missing window resize handles");
        std::function<int(QQuickItem *)> countHandles = [&](QQuickItem *item) {
          int count = item->objectName().startsWith("windowResize") && item->objectName()!="windowResizeHandles" ? 1 : 0;
          for (auto *child:item->childItems()) count+=countHandles(child);
          return count;
        };
        check(frame && countHandles(frame)==8,"Missing resize edges/corners");
        if(frame) check(frame->mapToScene(QPointF())==QPointF() && frame->size()==QSizeF(window->size()),"Resize handles do not cover the whole window");
        click(maximize); break;
      }
      case 1:
        check(window->visibility()==QWindow::Maximized,"Maximize button failed");
        check(frame && !frame->isVisible(),"Resize grips visible while maximized");
        check(maximize && maximize->property("text").toString()=="Restore","Maximize button did not become Restore");
        click(maximize); break;
      case 2:
        check(window->visibility()==QWindow::Windowed && window->size()==initialSize,"Restore button did not restore normal geometry");
        click(window->findChild<QQuickItem *>("windowDragArea"),true); break;
      case 3:
        check(window->visibility()==QWindow::Maximized,"Title-bar double-click failed");
        click(maximize); break;
      case 4:
        check(window->visibility()==QWindow::Windowed,"Normal state not restored");
        *minimized=false;
        click(window->findChild<QQuickItem *>("windowMinimizeButton")); break;
      case 5:
        // Wayland has no minimized configure flag; the compositor may report
        // Windowed immediately after accepting the native minimize request.
        check(*minimized,"Minimize button did not request the minimized state");
        window->showNormal(); break;
      case 6:
        check(window->visibility()==QWindow::Windowed,"Window did not restore after minimizing");
        window->grabWindow().save("evidence/custom-window.png");
        click(window->findChild<QQuickItem *>("windowCloseButton")); break;
      default: {
        check(!window->isVisible(),"Close button failed");
        QFile report("evidence/window-controls.json");
        if(report.open(QIODevice::WriteOnly)) report.write(QJsonDocument(QJsonObject{
          {"status",failures->isEmpty()?"PASS":"FAIL"},{"failures",*failures},
          {"checks",QJsonArray{"frameless","eight resize handles","maximize/restore",
              "double-click","minimize","close"}},
          {"all_checks_passed",failures->isEmpty()}}).toJson());
        timer->stop(); app.exit(failures->isEmpty()?0:1); break;
      }
      }
    });
    timer->start();
    QTimer::singleShot(10000,&app,[]{QCoreApplication::exit(3);});
  }
  if (smoke && !playlistSmoke && !dialogSmoke && !streamSmoke && !layoutSmoke && !memorySmoke && !windowSmoke) {
    QObject::connect(&player, &Controller::imported, &app, [&](int count) {
      if (count > 1 && player.playing())
        importWhilePlaying = true;
    });
    QObject::connect(&player, &Controller::loaded, &app, [&] {
      player.seek(12.5);
      player.play();
      audioStarted = player.playing();
      auto folder = qEnvironmentVariable("AYPLAYER_SMOKE_IMPORT");
      if (!folder.isEmpty())
        player.openFolder(QUrl::fromLocalFile(folder));
      QTimer::singleShot(1000, &app, [&] {
        positionAdvanced = player.position() > 12.5;
        player.pause();
        if (auto *area = window->findChild<QQuickItem *>("seekWaveform")) {
          auto sendMouse = [&](QEvent::Type type, double fraction,
                               Qt::MouseButton button,
                               Qt::MouseButtons buttons) {
            QPointF local = area->mapToScene(
                QPointF(area->width() * fraction, area->height() / 2));
            QMouseEvent event(type, local,
                              QPointF(window->mapToGlobal(local.toPoint())),
                              button, buttons, Qt::NoModifier);
            QCoreApplication::sendEvent(window, &event);
          };
          sendMouse(QEvent::MouseButtonPress, .25, Qt::LeftButton,
                    Qt::LeftButton);
          sendMouse(QEvent::MouseMove, .70, Qt::NoButton, Qt::LeftButton);
          sendMouse(QEvent::MouseButtonRelease, .70, Qt::LeftButton,
                    Qt::NoButton);
          mouseSeekVerified =
              std::abs(player.position() - player.duration() * .70) < .1;
        }
        player.seek(25.0);
        QTimer::singleShot(1000, &app, [&] {
          window->grabWindow().save("evidence/ui.png");
          window->resize(820, 680);
          QTimer::singleShot(150, &app, [&] {
            window->grabWindow().save("evidence/ui-compact.png");
            auto clickItem = [window](QQuickItem *item) {
              if (!item) return;
              QPointF scene = item->mapToScene(QPointF(item->width()/2,item->height()/2));
              QPointF global = window->mapToGlobal(scene.toPoint());
              QMouseEvent press(QEvent::MouseButtonPress, scene, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
              QMouseEvent release(QEvent::MouseButtonRelease, scene, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
              QCoreApplication::sendEvent(window,&press); QCoreApplication::sendEvent(window,&release);
            };
            clickItem(window->findChild<QQuickItem *>("listToolsButton"));
            QTimer::singleShot(150, &app, [&,clickItem] {
              auto *menu = window->findChild<QObject *>("playlistToolsMenu");
              auto *content = menu ? menu->property("contentItem").value<QQuickItem *>() : nullptr;
              listToolsVerified = menu && menu->property("visible").toBool()
                  && menu->property("width").toDouble()>=220 && menu->property("height").toDouble()>=150;
              window->grabWindow().save("evidence/list-tools.png");
              std::function<QQuickItem *(QQuickItem *)> findAction = [&](QQuickItem *item) -> QQuickItem * {
                if (!item) return nullptr;
                if (item->property("text").toString()=="Move down" && item->isEnabled()) return item;
                for (auto *child : item->childItems()) if (auto *match=findAction(child)) return match;
                return nullptr;
              };
              if (player.playlist().size()>1) {
                auto *action = findAction(content);
                const int current = player.currentTrack();
                clickItem(action);
                listToolsVerified &= action && player.currentTrack()==current+1;
              }
              if(menu) QMetaObject::invokeMethod(menu,"dismiss");
            window->resize(760, 520);
            QMetaObject::invokeMethod(window, "showMixer");
            QTimer::singleShot(150, &app, [&] {
              auto *slider = window->findChild<QQuickItem *>("gainARSlider");
              auto *gain = window->findChild<QQuickItem *>("gainAR");
              auto *channels = window->findChild<QQuickItem *>("mixerChannels");
              auto *output = window->findChild<QQuickItem *>("mixerOutput");
              auto *stereo = window->findChild<QQuickItem *>("mixerStereo");
              auto bounds = [](QQuickItem *item) {
                return QRectF(item->mapToScene(QPointF(0,0)), QSizeF(item->width(),item->height()));
              };
              if (slider && gain && channels && output && stereo) {
                int original = gain->property("value").toInt();
                slider->forceActiveFocus();
                QKeyEvent press(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
                QKeyEvent release(QEvent::KeyRelease, Qt::Key_Right, Qt::NoModifier);
                QCoreApplication::sendEvent(window, &press);
                QCoreApplication::sendEvent(window, &release);
                mixerVerified = gain->property("value").toInt() == std::min(255, original+1)
                    && bounds(channels).contains(bounds(stereo))
                    && !bounds(channels).intersects(bounds(output))
                    && QRectF(0,0,window->width(),window->height()).contains(bounds(output));
                QMetaObject::invokeMethod(window, "hideMixer");
                QMetaObject::invokeMethod(window, "showMixer");
                mixerVerified &= gain->property("value").toInt() == original
                    && player.gains()[1].toInt() == original;
              }
              window->grabWindow().save("evidence/ui-mixer.png");
              QMetaObject::invokeMethod(window, "hideMixer");
              player.play();
              player.exportWav(QUrl::fromLocalFile(
                  QFileInfo("evidence/gui-export.wav").absoluteFilePath()));
            });
            });
          });
        });
      });
    });
    QObject::connect(
        &player, &Controller::exported, &app, [&](const QString &error) {
          QFile f("evidence/gui-smoke.json");
          if (f.open(QIODevice::WriteOnly))
            f.write(
                QJsonDocument(
                    QJsonObject{{"export_error", error},
                                {"audio_started", audioStarted},
                                {"import_during_playback", importWhilePlaying},
                                {"underruns", player.underruns()},
                                {"export_start_cursor", 25.0},
                                {"playing_during_export", player.playing()},
                                {"position_advanced", positionAdvanced},
                                {"mouse_seek_verified", mouseSeekVerified},
                                {"compact_mixer_verified", mixerVerified},
                                {"list_tools_verified", listToolsVerified},
                                {"device", player.device()},
                                {"duration", player.duration()},
                                {"seek_position", player.position()},
                                {"status", player.status()}})
                    .toJson());
          player.stop();
          app.exit(error.isEmpty() && audioStarted && positionAdvanced &&
                           mouseSeekVerified && mixerVerified && listToolsVerified &&
                           (qEnvironmentVariableIsEmpty("AYPLAYER_SMOKE_IMPORT") || importWhilePlaying) &&
                           player.underruns() == 0
                       ? 0
                       : 1);
        });
    QTimer::singleShot(30000, &app, [] { QCoreApplication::exit(3); });
  }
  if (memorySmoke) {
    auto *switches = new int(0);
    auto *visualFrames = new int(0);
    auto *voices = new QJsonArray;
    const bool idle = qEnvironmentVariableIsSet("AYPLAYER_MEMORY_IDLE");
    const bool singleTrack = qEnvironmentVariableIsSet("AYPLAYER_MEMORY_SINGLE_TRACK");
    player.setVolume(0);
    if (singleTrack) player.setLoop(true);
    QObject::connect(&player, &Controller::loaded, &app, [&, switches, voices, idle] {
      ++*switches;
      if (!voices->contains(player.voiceCount())) voices->append(player.voiceCount());
      if (!idle) player.play();
    });
    QObject::connect(&player, &Controller::visualChanged, &app, [visualFrames] { ++*visualFrames; });
    auto *advance = new QTimer(&app);
    QObject::connect(advance, &QTimer::timeout, &app, [&] {
      if (!player.busy() && player.playlist().size()>1) player.next();
    });
    if (!idle && !singleTrack) advance->start(2000);
    QTimer::singleShot(0, &player, [&, idle] {
      QList<QUrl> files;
      for (const auto &path : paths) files.append(QUrl::fromLocalFile(path));
      player.openFiles(files);
      if (!idle && !paths.isEmpty()) player.openPath(paths.first());
      auto folder = qEnvironmentVariable("AYPLAYER_MEMORY_IMPORT");
      if (!folder.isEmpty()) player.openFolder(QUrl::fromLocalFile(folder));
    });
    const int requestedSeconds = qEnvironmentVariableIntValue("AYPLAYER_MEMORY_SECONDS");
    const int seconds = std::clamp(requestedSeconds ? requestedSeconds : 60, 10, 600);
    QTimer::singleShot(seconds*1000, &app, [&, switches, visualFrames, voices, idle, seconds, singleTrack] {
      QDir().mkpath("evidence");
      QFile report("evidence/memory-workload.json");
      bool passed = player.error().isEmpty() && !player.importing()
          && (idle || (*switches>=(singleTrack ? 1 : 3) && player.playing() && *visualFrames>100));
      if(report.open(QIODevice::WriteOnly)) report.write(QJsonDocument(QJsonObject{
          {"status", passed ? "PASS" : "FAIL"}, {"duration_seconds",seconds},
          {"idle",idle}, {"single_track",singleTrack}, {"tracks",player.playlist().size()}, {"track_loads",*switches},
          {"visual_updates",*visualFrames}, {"voice_counts",*voices},
          {"underruns",player.underruns()}, {"error",player.error()}}).toJson());
      player.stop();
      delete switches; delete visualFrames; delete voices;
      app.exit(passed ? 0 : 1);
    });
  }
  QElapsedTimer switchClock;
  QJsonArray transitionMs;
  int transitions = 0;
  bool outputAdvanced = true;
  if (streamSmoke) {
    switchClock.start();
    QObject::connect(&player, &Controller::loaded, &app, [&] {
      player.play();
      transitionMs.append(switchClock.nsecsElapsed() / 1000000.0);
      ++transitions;
      QTimer::singleShot(300, &app, [&] {
        outputAdvanced &= player.playing() && player.position() > .02;
        if (transitions == 1)
          window->grabWindow().save("evidence/channel-scopes.png");
        if (transitions < 6) {
          switchClock.restart();
          player.next();
          return;
        }
        QFile f("evidence/stream-device-smoke.json");
        if (f.open(QIODevice::WriteOnly))
          f.write(
              QJsonDocument(
                  QJsonObject{{"track_ready_ms", transitionMs},
                              {"output_advanced_each_track", outputAdvanced},
                              {"voices", player.voiceCount()},
                              {"scopes", int(player.channelScopes().size())},
                              {"underruns", player.underruns()},
                              {"device", player.device()},
                              {"error", player.error()}})
                  .toJson());
        player.stop();
        app.exit(outputAdvanced && player.error().isEmpty() ? 0 : 1);
      });
    });
    QTimer::singleShot(20000, &app, [] { QCoreApplication::exit(3); });
  }
  if (playlistSmoke) {
    auto *elapsed = new QElapsedTimer;
    elapsed->start();
    auto *loads = new int(0);
    QObject::connect(&player, &Controller::loaded, &app, [loads] { ++*loads; });
    QObject::connect(
        &player, &Controller::imported, &app, [&, elapsed, loads](int) {
          if (player.playlistModel()->rowCount() == 0)
            return;
          QTimer::singleShot(300, &app, [&, elapsed, loads] {
            window->grabWindow().save("evidence/playlist.png");
            int named = 0, timed = 0;
            auto *model = player.playlistModel();
            auto roles = model->roleNames();
            for (int i = 0; i < model->rowCount(); ++i) {
              auto title =
                  model->data(model->index(i, 0), roles.key("trackTitle"))
                      .toString();
              if (title != QFileInfo(player.playlist()[i]).completeBaseName())
                ++named;
              if (model->data(model->index(i, 0), roles.key("trackDuration"))
                      .toDouble() >= 0)
                ++timed;
            }
            QFile f("evidence/playlist-smoke.json");
            if (f.open(QIODevice::WriteOnly))
              f.write(
                  QJsonDocument(
                      QJsonObject{
                          {"tracks", model->rowCount()},
                          {"embedded_titles_different_from_filename", named},
                          {"durations", timed},
                          {"pcm_loads", *loads},
                          {"elapsed_ms", elapsed->elapsed()}})
                      .toJson());
            window->resize(820, 680);
            QTimer::singleShot(200, &app, [&, elapsed, loads] {
              window->grabWindow().save("evidence/playlist-compact.png");
              bool success = *loads == 0 && !player.busy();
              delete elapsed;
              delete loads;
              app.exit(success ? 0 : 1);
            });
          });
        });
    QTimer::singleShot(0, &player, [&player, paths] {
      if (!paths.isEmpty())
        player.openFolder(QUrl::fromLocalFile(paths.first()));
    });
    QTimer::singleShot(30000, &app, [] { QCoreApplication::exit(3); });
  } else if (dialogSmoke) {
    QSettings().setValue("browseDirectory",
                         paths.isEmpty() ? QDir::homePath() : paths.first());
    QTimer::singleShot(300, &app, [&] {
      player.browse("files");
      QTimer::singleShot(1200, &app, [&] {
        auto widgets = QApplication::topLevelWidgets();
        QJsonArray visibleWidgets;
        QWidget *capture = nullptr;
        QFileDialog *picker = nullptr;
        for (auto *widget : widgets) {
          if (auto *dialog = qobject_cast<QFileDialog *>(widget))
            picker = dialog;
          if (widget->isVisible()) {
            visibleWidgets.append(
                QString::fromLatin1(widget->metaObject()->className()));
            if (!capture || widget->width() * widget->height() >
                                capture->width() * capture->height())
              capture = widget;
          }
        }
        int selectedRows = 0;
        if (capture) {
          QAbstractItemView *files = nullptr;
          for (auto *view : capture->findChildren<QAbstractItemView *>()) {
            if (view->isVisible() && view->model() &&
                (!files ||
                 view->model()->rowCount() > files->model()->rowCount()))
              files = view;
          }
          if (files) {
            files->setFocus();
            QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier),
                release(QEvent::KeyRelease, Qt::Key_A, Qt::ControlModifier);
            QCoreApplication::sendEvent(files, &press);
            QCoreApplication::sendEvent(files, &release);
            if (files->selectionModel())
              selectedRows = files->selectionModel()->selectedRows().size();
          }
          capture->grab().save("evidence/native-dialog.png");
        }
        if (picker) {
          QFile f("evidence/native-dialog.json");
          if (f.open(QIODevice::WriteOnly))
            f.write(
                QJsonDocument(
                    QJsonObject{
                        {"platform_theme",
                         qEnvironmentVariable("QT_QPA_PLATFORMTHEME")},
                        {"native_allowed",
                         !picker->testOption(QFileDialog::DontUseNativeDialog)},
                        {"multi_select",
                         picker->fileMode() == QFileDialog::ExistingFiles},
                        {"visible_widgets", visibleWidgets},
                        {"ctrl_a_selected_rows", selectedRows}})
                    .toJson());
          picker->reject();
        }
        app.exit(0);
      });
    });
    QTimer::singleShot(5000, &app, [] { QCoreApplication::exit(3); });
  } else if (!memorySmoke && !paths.isEmpty()) {
    QList<QUrl> files;
    for (const auto &p : paths)
      files.append(QUrl::fromLocalFile(p));
    QTimer::singleShot(0, &player, [&player, files] {
      player.openFiles(files);
      player.openPath(files.first().toLocalFile());
    });
  }
  return app.exec();
}
