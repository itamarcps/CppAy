#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace ay {
struct Profile {
  int rate = 48000, clock = 1773400, preamp = 127, dmaGain = 146, filter = 1;
  double interruptHz = 50, maxSeconds = 600;
  uint64_t maxPcmBytes = 256ull * 1024 * 1024;
  bool ym = true, useFileTiming = true;
  std::array<int, 6> gains = {255, 13, 170, 170, 13, 255};
  void validate() const;
};
struct Event {
  uint64_t tick;
  uint32_t ordinal;
  uint8_t chip, reg, value;
  bool operator==(const Event &) const = default;
};
struct Song {
  std::string title, author, format;
  int version = 0;
  uint64_t interrupts = 0;
};
struct Render {
  Song song;
  Profile profile;
  std::vector<int16_t> pcm;
  std::vector<Event> events;
};
struct AudioChunk {
  uint64_t startFrame = 0;
  std::vector<int16_t> pcm;
  // Pre-pan chip voices, sampled on the PCM clock; zero unused TS voices.
  std::vector<std::array<int16_t, 6>> voices;
  std::vector<Event> events;
};
class StreamRenderer {
  struct Impl;
  std::unique_ptr<Impl> m;

public:
  explicit StreamRenderer(const std::filesystem::path &, const Profile & = {},
                          bool trace = false, bool captureVoices = false);
  StreamRenderer(const StreamRenderer &);
  StreamRenderer &operator=(const StreamRenderer &);
  StreamRenderer(StreamRenderer &&) noexcept;
  StreamRenderer &operator=(StreamRenderer &&) noexcept;
  ~StreamRenderer();
  const Song &song() const;
  const Profile &profile() const;
  uint64_t totalFrames() const;
  uint64_t position() const;
  int voiceCount() const;
  AudioChunk read(size_t frames);
};
using Progress = std::function<bool(double)>;
Render renderFile(const std::filesystem::path &, const Profile & = {},
                  Progress = {}, bool trace = false);
void writePsg(const std::filesystem::path &, const Render &);
void writeModuleFile(const std::filesystem::path &,
                     const std::vector<uint8_t> &);
void writeWav(const std::filesystem::path &, const Render &);
std::vector<uint8_t> readFile(const std::filesystem::path &);
} // namespace ay
