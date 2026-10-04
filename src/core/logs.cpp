// Register-log semantics ported from Sergey Bulba Players.pas.
#include "logs.h"
#include <stdexcept>
namespace ay {
namespace {
int masked(int reg, int value) {
  if (reg == 1 || reg == 3 || reg == 5 || reg == 13)
    return value & 15;
  if (reg == 6 || reg == 8 || reg == 9 || reg == 10)
    return value & 31;
  if (reg == 7)
    return value & 63;
  return value;
}
} // namespace
LogDecoder::LogDecoder(const std::vector<uint8_t> &data,
                       const std::string &title) {
  song.title = title;
  if (data.size() >= 16 && std::string(data.begin(), data.begin() + 4) ==
                               std::string("PSG\x1a", 4)) {
    if (data[4] > 10)
      throw std::runtime_error(
          "Unsupported PSG version (reference supports 0..10)");
    song.version = data[4];
    song.format = "PSG";
    size_t pos = 16;
    uint64_t duration = 0;
    int last = 0;
    // Reference duration scan includes a final EOM or unterminated write frame.
    while (pos < data.size()) {
      last = data[pos++];
      if (last == 255)
        ++duration;
      else if (last == 254) {
        if (pos >= data.size())
          throw std::runtime_error("Truncated PSG skip marker");
        duration += data[pos++] * 4;
      } else if (last == 253)
        break;
      else {
        if (pos >= data.size())
          throw std::runtime_error("Truncated PSG register pair");
        ++pos;
      }
    }
    if (last != 254 && last != 255)
      ++duration;
    if (!duration || duration > 600000)
      throw std::runtime_error("Empty/oversized PSG timeline");
    frames.resize(duration);
    pos = 16;
    int skip = 0;
    for (auto &frame : frames) {
      if (skip > 0) {
        --skip;
        continue;
      }
      if (pos >= data.size() || skip < 0) {
        pos = 16;
        skip = 0; /* no-loop duration should normally end before restart */
      }
      while (pos < data.size()) {
        int reg = data[pos++];
        if (reg == 255)
          break;
        if (reg == 254) {
          if (pos >= data.size())
            throw std::runtime_error("Truncated PSG skip marker");
          skip = data[pos++] * 4 - 1;
          break;
        }
        if (reg == 253) {
          skip = -1;
          break;
        }
        if (pos >= data.size())
          throw std::runtime_error("Truncated PSG register pair");
        int value = data[pos++];
        if (reg < 14)
          frame.emplace_back(reg, masked(reg, value));
      }
    }
  } else if (data.size() >= 18 &&
             (std::string(data.begin(), data.begin() + 4) == "YM3!" ||
              std::string(data.begin(), data.begin() + 4) == "YM3b")) {
    bool loop = std::string(data.begin(), data.begin() + 4) == "YM3b";
    size_t bytes = data.size() - (loop ? 8 : 4);
    if (!bytes || bytes % 14)
      throw std::runtime_error("Invalid YM3 register-plane size");
    size_t count = bytes / 14;
    if (count > 600000)
      throw std::runtime_error("YM3 timeline exceeds limit");
    song.format = loop ? "YM3b" : "YM3";
    frames.resize(count);
    for (size_t i = 0; i < count; ++i)
      for (int reg = 0; reg < 14; ++reg) {
        int value = data[4 + reg * count + i];
        if (reg == 13 && value == 255)
          continue;
        frames[i].emplace_back(reg, masked(reg, value));
      }
  } else
    throw std::runtime_error("Unsupported or unrecognized format; implemented "
                             "native paths: PT3, PSG, YM3/YM3b");
  song.interrupts = frames.size();
}
} // namespace ay
