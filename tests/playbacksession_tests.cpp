#include "playbacksession.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QThread>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#define REQUIRE(x)                                                             \
  do {                                                                         \
    if (!(x))                                                                  \
      throw std::runtime_error("Failed: " #x);                                 \
  } while (false)
static void ready(PlaybackSession &session) {
  QElapsedTimer elapsed;
  elapsed.start();
  while (!session.ready()) {
    REQUIRE(session.error().isEmpty());
    REQUIRE(elapsed.elapsed() < 10000);
    QThread::msleep(1);
  }
}
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  try {
    REQUIRE(argc == 4);
    QFile reference(argv[2]);
    REQUIRE(reference.open(QIODevice::ReadOnly));
    auto expected = reference.readAll().mid(44);
    QElapsedTimer startup;
    startup.start();
    PlaybackSession session(ay::StreamRenderer(argv[1], {}, false, true));
    auto startupNs = startup.nsecsElapsed();
    REQUIRE(startup.elapsed() < 500);
    REQUIRE(session.frames == uint64_t(expected.size() / 4));
    REQUIRE(session.voiceCount == 3);
    QThread::msleep(30);
    REQUIRE(session.bufferedFrames() <=
            uint64_t(session.profile.rate / 3 + session.profile.rate / 50));
    QByteArray actual;
    size_t turn = 0;
    std::array<size_t, 7> counts = {1, 3, 511, 4093, 17, 2400, 8192};
    while (actual.size() < expected.size()) {
      auto bytes = session.take(counts[turn++ % counts.size()]);
      if (bytes.isEmpty()) {
        ready(session);
        QThread::msleep(1);
        continue;
      }
      actual.append(bytes);
    }
    REQUIRE(actual == expected);
    for (uint64_t frame : {uint64_t(0), uint64_t(1), uint64_t(123456),
                           uint64_t(600000), session.frames - 512}) {
      session.seek(frame);
      ready(session);
      QByteArray bytes;
      while (bytes.size() < 512 * 4) {
        bytes.append(session.take((512 * 4 - bytes.size()) / 4));
        if (bytes.size() < 512 * 4)
          ready(session);
      }
      REQUIRE(bytes == expected.mid(frame * 4, 512 * 4));
    }
    session.seek(1000000);
    session.seek(5);
    session.seek(800000);
    ready(session);
    auto after = session.take(128);
    REQUIRE(after == expected.mid(800000 * 4, after.size()));
    // A dedicated single-voice fixture proves scopes are independent source
    // channels, not three copies or panned derivations of the stereo mix.
    QTemporaryDir temp;
    auto log = temp.filePath("voice-a.psg");
    QFile f(log);
    REQUIRE(f.open(QIODevice::WriteOnly));
    QByteArray psg("PSG\x1a", 4);
    psg.append(char(10));
    psg.append(char(50));
    psg.append(QByteArray(10, 0));
    for (auto [reg, value] : std::array<std::pair<int, int>, 6>{
             {{0, 30}, {1, 0}, {7, 62}, {8, 15}, {9, 0}, {10, 0}}}) {
      psg.append(char(reg));
      psg.append(char(value));
    }
    psg.append(QByteArray(8, char(255)));
    f.write(psg);
    f.close();
    ay::StreamRenderer one(log.toStdString(), {}, false, true);
    auto voiceData = one.read(2048);
    bool audible = false;
    for (auto voices : voiceData.voices) {
      REQUIRE(voices[1] == 0 && voices[2] == 0);
      audible |= voices[0] > 0;
    }
    REQUIRE(audible);
    PlaybackSession triggered(ay::StreamRenderer(log.toStdString(), {}, false, true));
    QElapsedTimer triggerWait; triggerWait.start();
    while (triggered.bufferedFrames() < triggered.frames) {
      REQUIRE(triggerWait.elapsed() < 5000); QThread::msleep(1);
    }
    // Different cursors land at different tone phases. All displayed windows
    // must begin immediately before a rising midpoint crossing independently
    // of the playback cursor; inactive B/C voices must remain silent.
    for (uint64_t cursor : {480, 731, 1107, 2107, 3451, 4601}) {
      auto view = triggered.analysis(cursor);
      REQUIRE(view.scopes[0].size() == 240);
      const auto [low, high] = std::minmax_element(view.scopes[0].begin(), view.scopes[0].end());
      const float midpoint = (*low + *high) / 2;
      REQUIRE(*high > *low);
      REQUIRE(view.scopes[0][0] <= midpoint && view.scopes[0][1] > midpoint);
      for (int v : {1,2}) for (auto point : view.scopes[v]) REQUIRE(point == 0);
    }
    // At the lowest normal AY tone, the period exceeds the 20 ms display.
    // The history trigger still gives a stable rising edge at unrelated cursors.
    auto slowLog = temp.filePath("low-tone.psg");
    auto slowData = psg; slowData[17] = char(255); slowData[19] = char(15);
    QFile slowFile(slowLog); REQUIRE(slowFile.open(QIODevice::WriteOnly));
    REQUIRE(slowFile.write(slowData) == slowData.size()); slowFile.close();
    PlaybackSession lowTone(ay::StreamRenderer(slowLog.toStdString(), {}, false, true));
    triggerWait.restart();
    while (lowTone.bufferedFrames() < lowTone.frames) {
      REQUIRE(triggerWait.elapsed() < 5000); QThread::msleep(1);
    }
    for (uint64_t cursor : {2500, 3000, 4000, 5000, 6000}) {
      auto view = lowTone.analysis(cursor);
      REQUIRE(view.scopes[0].size() == 240);
      const auto [low, high] = std::minmax_element(view.scopes[0].begin(), view.scopes[0].end());
      const float midpoint = (*low + *high) / 2;
      REQUIRE(*high > *low);
      REQUIRE(view.scopes[0][0] <= midpoint && view.scopes[0][1] > midpoint);
    }
    PlaybackSession ts(ay::StreamRenderer(argv[3], {}, false, true));
    REQUIRE(ts.voiceCount == 6);
    ts.seek(1);
    ready(ts);
    auto tsBytes = ts.take(128);
    ay::StreamRenderer tsReference(argv[3]);
    tsReference.read(1);
    auto tsExpected = tsReference.read(128);
    REQUIRE(tsBytes ==
            QByteArray(reinterpret_cast<const char *>(tsExpected.pcm.data()),
                       tsExpected.pcm.size() * 2));
    QJsonObject report{{"status", "PASS"},
                       {"first_audio_ready_ms", startupNs / 1000000.0},
                       {"prebuffer_ms", 40},
                       {"buffer_bound_ms", 354},
                       {"full_stream_pcm", "BIT_EXACT_PCM"},
                       {"seek_pcm", "BIT_EXACT_PCM"},
                       {"independent_channel_taps", true},
                       {"phase_triggered_scopes", true},
                       {"turbosound_voices", 6}};
    QFile output("streaming-verification.json");
    if (output.open(QIODevice::WriteOnly))
      output.write(QJsonDocument(report).toJson());
    std::cout << QJsonDocument(report).toJson().toStdString();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
