#pragma once
#include "engine.h"
namespace ay {
struct LogDecoder {
  Song song;
  std::vector<std::vector<std::pair<int, int>>> frames;
  explicit LogDecoder(const std::vector<uint8_t> &, const std::string &);
};
} // namespace ay
