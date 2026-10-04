#include "playbacksession.h"
#include <algorithm>
#include <cmath>

PlaybackSession::PlaybackSession(ay::StreamRenderer engine)
    : song(engine.song()), profile(engine.profile()),
      frames(engine.totalFrames()), voiceCount(engine.voiceCount()) {
  // ~40 ms is enough to begin output; later blocks are generated on demand.
  ay::StreamRenderer origin(engine);
  auto first = std::make_shared<ay::AudioChunk>(engine.read(profile.rate / 25));
  queuedFrames = first->pcm.size() / 2;
  queue.push_back(first);
  history.push_back(first);
  producer = std::thread(
      [this, engine = std::move(engine), origin = std::move(origin)]() mutable {
        run(std::move(engine), std::move(origin));
      });
}
PlaybackSession::~PlaybackSession() {
  {
    std::lock_guard lock(mutex);
    stopping = true;
    ++generation;
  }
  changed.notify_all();
  if (producer.joinable())
    producer.join();
}
bool PlaybackSession::ready() const {
  std::lock_guard lock(mutex);
  return generation == activeGeneration &&
         (queuedFrames > 0 || requestedFrame >= frames);
}
QString PlaybackSession::error() const {
  std::lock_guard lock(mutex);
  return failure;
}
uint64_t PlaybackSession::bufferedFrames() const {
  std::lock_guard lock(mutex);
  return queuedFrames;
}
QByteArray PlaybackSession::take(size_t count) {
  std::lock_guard lock(mutex);
  QByteArray bytes;
  if (generation != activeGeneration)
    return bytes;
  count = std::min<uint64_t>(count, queuedFrames);
  bytes.reserve(count * 4);
  while (count && !queue.empty()) {
    const auto &block = *queue.front();
    size_t n = std::min(count, block.pcm.size() / 2 - readOffset);
    bytes.append(
        reinterpret_cast<const char *>(block.pcm.data() + readOffset * 2),
        n * 4);
    readOffset += n;
    count -= n;
    queuedFrames -= n;
    if (readOffset == block.pcm.size() / 2) {
      queue.pop_front();
      readOffset = 0;
    }
  }
  changed.notify_all();
  return bytes;
}
void PlaybackSession::seek(uint64_t frame) {
  std::lock_guard lock(mutex);
  requestedFrame = std::min(frame, frames);
  ++generation;
  queue.clear();
  history.clear();
  queuedFrames = 0;
  readOffset = 0;
  failure.clear();
  changed.notify_all();
}
void PlaybackSession::run(ay::StreamRenderer engine,
                          ay::StreamRenderer origin) {
  // A small, bounded collection of complete chip/filter/decoder checkpoints
  // makes backward seeks cheap while retaining exact state.
  std::map<uint64_t, ay::StreamRenderer> checkpoints;
  checkpoints.emplace(0, origin);
  checkpoints.emplace(engine.position(), engine);
  uint64_t nextCheckpoint = profile.rate;
  while (true) {
    uint64_t desiredGeneration, target;
    {
      std::unique_lock lock(mutex);
      changed.wait(lock, [&] {
        return stopping || generation != activeGeneration ||
               (queuedFrames < uint64_t(profile.rate / 3) &&
                engine.position() < frames);
      });
      if (stopping)
        return;
      desiredGeneration = generation;
      target = requestedFrame;
    }
    try {
      if (desiredGeneration != activeGeneration) {
        auto it = checkpoints.upper_bound(target);
        if (it == checkpoints.begin())
          engine = origin;
        else {
          --it;
          engine = it->second;
        }
        // A seek before the prebuffer requires the exact frame-zero state.
        if (engine.position() > target)
          throw std::runtime_error("Missing frame-zero checkpoint");
        while (engine.position() < target) {
          {
            std::lock_guard lock(mutex);
            if (stopping)
              return;
            if (generation != desiredGeneration)
              break;
          }
          engine.read(std::min<uint64_t>(4096, target - engine.position()));
        }
        {
          std::lock_guard lock(mutex);
          if (generation != desiredGeneration)
            continue;
          activeGeneration = desiredGeneration;
        }
        nextCheckpoint = engine.position() + profile.rate;
      }
      auto block =
          std::make_shared<ay::AudioChunk>(engine.read(profile.rate / 50));
      if (engine.position() >= nextCheckpoint) {
        checkpoints.insert_or_assign(engine.position(), engine);
        nextCheckpoint = engine.position() + profile.rate;
        while (checkpoints.size() > 64)
          checkpoints.erase(std::next(checkpoints.begin()));
      }
      std::lock_guard lock(mutex);
      if (generation != desiredGeneration)
        continue;
      if (!block->pcm.empty()) {
        queuedFrames += block->pcm.size() / 2;
        queue.push_back(block);
        history.push_back(block);
        for (size_t i = 0; i < block->pcm.size() / 2; ++i) {
          size_t bin =
              std::min<uint64_t>(255, (block->startFrame + i) * 256 /
                                          std::max<uint64_t>(1, frames));
          overview[bin] =
              std::max(overview[bin],
                       float(std::max(std::abs(int(block->pcm[i * 2])),
                                      std::abs(int(block->pcm[i * 2 + 1]))) /
                             32768));
        }
        while (history.size() > 2 &&
               history.front()->startFrame + profile.rate * 2 <
                   block->startFrame)
          history.pop_front();
      }
    } catch (const std::exception &e) {
      std::unique_lock lock(mutex);
      failure = QString::fromUtf8(e.what());
      changed.wait(lock,
                   [&] { return stopping || generation != desiredGeneration; });
      if (stopping)
        return;
    }
  }
}
PlaybackSession::Analysis PlaybackSession::analysis(uint64_t frame) const {
  Analysis result;
  std::lock_guard lock(mutex);
  if (generation != activeGeneration)
    return result;
  // Trigger each 20 ms display on its voice's most recent rising edge.
  // A bounded 100 ms lookback covers low AY tones without showing future notes.
  const uint64_t window = profile.rate / 50;
  const uint64_t begin = frame > uint64_t(profile.rate / 10) ? frame - profile.rate / 10 : 0;
  const uint64_t end = frame + window;
  uint64_t capturedStart = frame;
  bool captured = false;
  std::array<std::vector<int16_t>, 6> samples;
  for (int v = 0; v < voiceCount; ++v) samples[v].reserve(end - begin);
  for (const auto &block : history) {
    uint64_t blockEnd = block->startFrame + block->pcm.size() / 2;
    if (blockEnd <= begin || block->startFrame >= end) continue;
    const size_t first = begin > block->startFrame ? begin - block->startFrame : 0;
    if (!captured) { capturedStart = block->startFrame + first; captured = true; }
    const size_t last = std::min<uint64_t>(block->pcm.size() / 2, end - block->startFrame);
    for (size_t i = first; i < last; ++i) {
      if (block->startFrame + i >= frame && block->startFrame + i < frame + window) {
        result.left = std::max(result.left, std::abs(block->pcm[i * 2] / 32768.0));
        result.right = std::max(result.right, std::abs(block->pcm[i * 2 + 1] / 32768.0));
      }
      if (i < block->voices.size())
        for (int v = 0; v < voiceCount; ++v) samples[v].push_back(block->voices[i][v]);
    }
  }
  for (int v = 0; v < voiceCount; ++v) {
    const auto &raw = samples[v];
    if (raw.empty()) continue;
    const auto [low, high] = std::minmax_element(raw.begin(), raw.end());
    const double level = (int(*low) + int(*high)) / 2.0;
    const size_t current = std::min<uint64_t>(raw.size() - 1, frame > capturedStart ? frame - capturedStart : 0);
    size_t start = current;
    if (int(*high) - int(*low) > 32 && raw.size() > window) {
      const size_t latest = std::min<size_t>(current, raw.size() - window);
      bool triggered = false;
      for (size_t i = latest; i > 0; --i) {
        if (raw[i - 1] <= level && raw[i] > level) {
          start = i - 1; triggered = true; break;
        }
      }
      // At startup/after seeking there may not be any past samples yet.
      if (!triggered) {
        for (size_t i = latest + 1; i <= raw.size() - window; ++i) {
          if (raw[i - 1] <= level && raw[i] > level) { start = i - 1; break; }
        }
      }
    }
    const size_t count = std::min<size_t>(window, raw.size() - start);
    result.scopes[v].reserve((count + 3) / 4);
    for (size_t i = 0; i < count; i += 4)
      result.scopes[v].push_back(raw[start + i] / 32768.0f);
  }
  return result;
}
QVariantList PlaybackSession::waveform() const {
  std::lock_guard lock(mutex);
  QVariantList result;
  result.reserve(256);
  for (auto peak : overview)
    result.append(peak);
  return result;
}
