#include "engine.h"
#include "pt3.h"
#include <algorithm>
#include <future>
#include <iostream>
#include <stdexcept>
#define REQUIRE(x)                                                             \
  do {                                                                         \
    if (!(x))                                                                  \
      throw std::runtime_error("Failed: " #x);                                 \
  } while (false)
int main(int argc, char **argv) {
  try {
    REQUIRE(argc == 2);
    auto path = std::filesystem::path(argv[1]);
    auto golden = ay::renderFile(path, {}, {}, true);
    REQUIRE(golden.pcm.size() == 4915758);
    REQUIRE(golden.song.interrupts == 2560);
    REQUIRE(!golden.events.empty());
    auto second =
        std::async(std::launch::async, [&] { return ay::renderFile(path); });
    auto first = ay::renderFile(path);
    REQUIRE(first.pcm == golden.pcm);
    REQUIRE(second.get().pcm == golden.pcm);
    // Exercise actual incremental synthesis, including odd/single-frame reads,
    // observer taps and mid-interrupt checkpoint copies, against canonical PCM.
    ay::StreamRenderer stream(path, {}, true, true);
    REQUIRE(stream.totalFrames() == golden.pcm.size() / 2);
    std::vector<int16_t> consumed;
    std::vector<ay::Event> streamedEvents;
    size_t turn = 0;
    const std::array<size_t, 8> chunks = {1, 3, 17, 511, 48000, 4093, 73, 8192};
    bool voicesDiffer = false;
    while (stream.position() < stream.totalFrames()) {
      auto block = stream.read(chunks[turn++ % chunks.size()]);
      REQUIRE(block.voices.size() == block.pcm.size() / 2);
      for (auto voices : block.voices)
        if (voices[0] != voices[1] || voices[1] != voices[2])
          voicesDiffer = true;
      consumed.insert(consumed.end(), block.pcm.begin(), block.pcm.end());
      streamedEvents.insert(streamedEvents.end(), block.events.begin(),
                            block.events.end());
      if (turn == 4) {
        ay::StreamRenderer checkpoint(stream);
        auto a = checkpoint.read(1237);
        ay::StreamRenderer again(stream);
        auto b = again.read(1237);
        REQUIRE(a.pcm == b.pcm);
        REQUIRE(a.voices == b.voices);
      }
    }
    REQUIRE(consumed == golden.pcm);
    REQUIRE(streamedEvents == golden.events);
    REQUIRE(voicesDiffer);
    bool cancelled = false;
    try {
      ay::renderFile(path, {}, [](double) { return false; });
    } catch (const std::exception &e) {
      cancelled = std::string(e.what()) == "Cancelled";
    }
    REQUIRE(cancelled);
    auto bytes = ay::readFile(path);
    for (size_t n :
         {size_t(0), size_t(10), size_t(100), size_t(201), size_t(300)}) {
      bool rejected = false;
      try {
        ay::Pt3 p(std::vector<uint8_t>(bytes.begin(), bytes.begin() + n));
        while (p.frame()) {
        };
      } catch (...) {
        rejected = true;
      }
      REQUIRE(rejected);
    }
    auto corrupt = bytes;
    corrupt[100] = 0;
    bool rejected = false;
    try {
      ay::Pt3 p(corrupt);
    } catch (...) {
      rejected = true;
    }
    REQUIRE(rejected);
    corrupt = bytes;
    corrupt[103] = 255;
    corrupt[104] = 255;
    rejected = false;
    try {
      ay::Pt3 p(corrupt);
    } catch (...) {
      rejected = true;
    }
    REQUIRE(rejected);
    ay::Profile p;
    p.rate = 7999;
    rejected = false;
    try {
      p.validate();
    } catch (...) {
      rejected = true;
    }
    REQUIRE(rejected);
    p = {};
    p.preamp = 0;
    auto silence = ay::renderFile(path, p);
    REQUIRE(std::all_of(silence.pcm.begin(), silence.pcm.end(),
                        [](auto v) { return v == 0; }));
    p = {};
    p.filter = 0;
    auto noFilter = ay::renderFile(path, p);
    REQUIRE(noFilter.pcm.size() == first.pcm.size());
    REQUIRE(noFilter.pcm != first.pcm);
    std::cout << "Core: deterministic/concurrent rendering, canonical PCM "
                 "consumption, cancellation, bounds, profile validation and "
                 "silence passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
