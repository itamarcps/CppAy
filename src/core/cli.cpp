#include "engine.h"
#include <fstream>
#include <iostream>
namespace {
std::filesystem::path argumentPath(const char *text) {
#ifdef _WIN32
  return std::filesystem::path(reinterpret_cast<const char8_t *>(text));
#else
  return std::filesystem::path(text);
#endif
}
} // namespace

int run(int argc, char **argv) {
  try {
    if (argc < 3) {
      std::cerr << "Usage: aytool inspect FILE | render FILE OUTPUT.wav | psg "
                   "FILE OUTPUT.psg | trace FILE OUTPUT.jsonl [--ay "
                   "--no-filter --rate N --clock N --interrupt N --preamp N]\n";
      return 2;
    }
    std::string command = argv[1];
    ay::Profile p;
    int start = command == "inspect" ? 3 : 4;
    for (int i = start; i < argc; ++i) {
      std::string a = argv[i];
      auto value = [&]() {
        if (++i >= argc)
          throw std::runtime_error("Missing option value");
        return std::string(argv[i]);
      };
      if (a == "--ay")
        p.ym = false;
      else if (a == "--no-filter")
        p.filter = 0;
      else if (a == "--rate")
        p.rate = std::stoi(value());
      else if (a == "--clock")
        p.clock = std::stoi(value());
      else if (a == "--interrupt") {
        p.interruptHz = std::stod(value());
        p.useFileTiming = false;
      } else if (a == "--max-seconds")
        p.maxSeconds = std::stod(value());
      else if (a == "--memory-mib")
        p.maxPcmBytes = std::stoull(value()) * 1024 * 1024;
      else if (a == "--preamp")
        p.preamp = std::stoi(value());
      else
        throw std::runtime_error("Unknown option: " + a);
    }
    if (command != "inspect" && command != "render" && command != "trace" &&
        command != "psg")
      throw std::runtime_error("Unknown command");
    if (command != "inspect" && argc < 4)
      throw std::runtime_error("Missing output path");
    auto r =
        ay::renderFile(argumentPath(argv[2]), p, {}, command == "trace" || command == "psg");
    if (command == "psg")
      ay::writePsg(argumentPath(argv[3]), r);
    if (command == "render")
      ay::writeWav(argumentPath(argv[3]), r);
    if (command == "trace") {
      if (std::filesystem::exists(argumentPath(argv[3])))
        throw std::runtime_error("Trace destination already exists");
      std::ofstream f(argumentPath(argv[3]));
      if (!f)
        throw std::runtime_error("Cannot create trace");
      for (auto e : r.events)
        f << "{\"tick\":" << e.tick << ",\"ordinal\":" << e.ordinal
          << ",\"chip\":" << int(e.chip) << ",\"register\":" << int(e.reg)
          << ",\"value\":" << int(e.value) << "}\n";
    }
    std::cout << r.song.format << " v" << r.song.version << ": " << r.song.title
              << " by " << r.song.author << "\n"
              << r.song.interrupts << " interrupts, " << r.pcm.size() / 2
              << " stereo frames at " << p.rate << " Hz\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
  std::vector<std::string> storage;
  storage.reserve(argc);
  for (int i = 0; i < argc; ++i) {
    auto utf8 = std::filesystem::path(argv[i]).u8string();
    storage.emplace_back(utf8.begin(), utf8.end());
  }
  std::vector<char *> arguments;
  for (auto &argument : storage)
    arguments.push_back(argument.data());
  return run(argc, arguments.data());
}
#else
int main(int argc, char **argv) { return run(argc, argv); }
#endif
