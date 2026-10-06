#include "engine.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      throw std::runtime_error("Two fixtures required");
    for (int i = 1; i < argc; ++i) {
      auto canonical = ay::renderFile(argv[i], {}, {}, true);
      ay::StreamRenderer stream(argv[i], {}, true, true);
      auto prefix = stream.read(37);
      auto checkpoint = stream;
      if (checkpoint.read(503).pcm != stream.read(503).pcm)
        throw std::runtime_error("Legacy checkpoint divergence");
      ay::StreamRenderer odd(argv[i], {}, true, true);
      std::vector<int16_t> pcm;
      std::vector<ay::Event> events;
      for (size_t n = 0; odd.position() < odd.totalFrames(); ++n) {
        auto chunk = odd.read(n % 2 ? 511 : 3);
        pcm.insert(pcm.end(), chunk.pcm.begin(), chunk.pcm.end());
        events.insert(events.end(), chunk.events.begin(), chunk.events.end());
        if (chunk.voices.size() != chunk.pcm.size() / 2)
          throw std::runtime_error("Voice capture size mismatch");
      }
      if (pcm != canonical.pcm || events != canonical.events)
        throw std::runtime_error("Chunk-size dependence");
      auto a = checkpoint, b = checkpoint;
      if (a.read(71).pcm != b.read(71).pcm)
        throw std::runtime_error("Independent-instance contamination");
    }
    std::cout << "PASS: PT2/STC streaming, checkpoint copies, chunk sizes, "
                 "events and voice taps\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
