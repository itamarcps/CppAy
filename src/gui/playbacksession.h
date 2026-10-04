#pragma once
#include "engine.h"
#include <QByteArray>
#include <QVariantList>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

// Producer owns the decoder and synth. GUI only consumes bounded PCM and
// snapshots; neither the audio device nor the GUI ever renders a full song.
class PlaybackSession {
public:
  struct Analysis {
    std::array<std::vector<float>, 6> scopes;
    double left = 0, right = 0;
  };
  const ay::Song song;
  const ay::Profile profile;
  const uint64_t frames;
  const int voiceCount;
  explicit PlaybackSession(ay::StreamRenderer);
  ~PlaybackSession();
  PlaybackSession(const PlaybackSession &) = delete;
  bool ready() const;
  QString error() const;
  QByteArray take(size_t maxFrames);
  void seek(uint64_t frame);
  Analysis analysis(uint64_t frame) const;
  QVariantList waveform() const;
  uint64_t bufferedFrames() const;

private:
  mutable std::mutex mutex;
  std::condition_variable changed;
  std::thread producer;
  bool stopping = false;
  uint64_t generation = 0, requestedFrame = 0, activeGeneration = 0,
           queuedFrames = 0;
  size_t readOffset = 0;
  QString failure;
  std::deque<std::shared_ptr<ay::AudioChunk>> queue, history;
  std::array<float, 256> overview{};
  void run(ay::StreamRenderer, ay::StreamRenderer);
};
