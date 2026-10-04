#pragma once
#include "engine.h"
#include <bit>
#include <stdexcept>
namespace ay {
inline int s8(int x) { return std::bit_cast<int8_t>(uint8_t(x)); }
inline int s16(int x) { return std::bit_cast<int16_t>(uint16_t(x)); }
struct Pt3 {
  struct Channel {
    uint16_t address = 0, ornament = 0, sample = 0, tone = 0;
    uint8_t ornLoop = 0, ornLength = 0, ornPos = 0, smpLoop = 0, smpLength = 0,
            smpPos = 0, volume = 15, skip = 0, note = 0, target = 0, amp = 0;
    bool envelope = false, enabled = false, gliss = false;
    int16_t ampSlide = 0, slideCount = 0, onoff = 0, onDelay = 0, offDelay = 0,
            slideDelay = 0, slide = 0, accum = 0, step = 0, delta = 0;
    int8_t skipCount = 1;
    uint8_t noiseSlide = 0, envSlide = 0;
  };
  std::vector<uint8_t> data;
  std::array<Channel, 3> channels;
  std::array<uint8_t, 14> regs{};
  Song song;
  int delayCounter = 1, delay = 0, position = 0;
  uint8_t noiseBase = 0, addNoise = 0;
  int16_t envBase = 0, envSlide = 0, envAdd = 0;
  int8_t envDelay = 0, envCounter = 0;
  std::vector<std::pair<int, int>> writes;
  Pt3 *partner = nullptr;
  bool mirrored = false, allowPatternLoop = false;
  explicit Pt3(std::vector<uint8_t>, bool mirror = false);
  int byte(size_t i) const {
    if (i >= data.size())
      throw std::runtime_error("PT3 read outside module at " +
                               std::to_string(i));
    return data[i];
  }
  int word(size_t i) const { return byte(i) | (byte(i + 1) << 8); }
  int frequency(int) const;
  void setSample(Channel &, int);
  void setOrnament(Channel &, int);
  void patterns();
  void interpret(Channel &);
  void change(Channel &, uint8_t &, int8_t &);
  bool frame();
  void write(int r, int v) {
    regs.at(r) = uint8_t(v);
    writes.emplace_back(r, uint8_t(v));
  }
};
uint64_t pt3Duration(const Pt3 &, bool turbo = false);
} // namespace ay
