#pragma once
#include "pt3.h"
namespace ay {
// Native ports of AY_Emul's PT2 and STC players; see THIRD_PARTY.md.
struct Legacy {
  struct Channel {
    size_t address = 0, sample = 0, ornament = 0;
    uint16_t tone = 0;
    uint8_t note = 0, target = 0, volume = 15, skip = 0, smpPos = 0, ornPos = 0,
            smpLength = 0, smpLoop = 0, ornLength = 0, ornLoop = 0, amp = 0;
    int8_t count = 0, sampleTicks = -1, gliss = 0, noise = 0;
    int16_t slide = 0, delta = 0;
    int glissType = 0;
    bool enabled = false, envelope = false;
  };
  std::vector<uint8_t> data;
  std::array<Channel, 3> channels;
  std::array<uint8_t, 14> regs{};
  std::vector<std::pair<int, int>> writes;
  Song song;
  bool stc = false;
  int delay = 0, counter = 1, position = 0, positions = 0, transpose = 0;
  explicit Legacy(std::vector<uint8_t>, bool soundTracker);
  int byte(size_t) const;
  int word(size_t p) const { return byte(p) | (byte(p + 1) << 8); }
  void patterns();
  void sample(Channel &, int);
  void ornament(Channel &, int);
  void interpret(Channel &);
  void change(Channel &, uint8_t &);
  bool frame();
  uint64_t duration(uint64_t budget = 3000000) const;
  void write(int r, int v) {
    regs.at(r) = uint8_t(v);
    writes.emplace_back(r, uint8_t(v));
  }
};
void convertToPt3(const std::filesystem::path &, const std::filesystem::path &);
} // namespace ay
