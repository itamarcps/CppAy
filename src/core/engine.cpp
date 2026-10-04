// AY synthesis/mixer/FIR ported from Sergey Bulba AY_Emul AY.pas and
// MainWin.pas.
#include "engine.h"
#include "logs.h"
#include "pt3.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <numbers>
#include <stdexcept>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace ay {
namespace {
void commitExport(const std::filesystem::path &temporary,
                  const std::filesystem::path &destination) {
#ifdef _WIN32
  // A same-directory rename works on NTFS and removable FAT/exFAT drives.
  // No REPLACE_EXISTING flag: a racing destination must never be overwritten.
  if (!MoveFileExW(temporary.c_str(), destination.c_str(), 0))
    throw std::system_error(int(GetLastError()), std::system_category(),
                            "Cannot finish export");
#else
  std::filesystem::create_hard_link(temporary, destination);
  std::filesystem::remove(temporary);
#endif
}
} // namespace

void Profile::validate() const {
  if (!std::isfinite(maxSeconds) || maxSeconds <= 0 || maxSeconds > 86400 ||
      maxPcmBytes < 1024)
    throw std::runtime_error("Invalid rendering resource budget");
  if (rate < 8000 || rate >= 300000 || clock < 1000000 || clock > 3546800 ||
      !std::isfinite(interruptHz) || interruptHz < 1 || interruptHz > 2000 ||
      preamp < 0 || preamp > 255 || dmaGain < 0 || dmaGain > 255 ||
      filter < 0 || filter > 1)
    throw std::runtime_error("Invalid render profile");
  for (int g : gains)
    if (g < 0 || g > 255)
      throw std::runtime_error("Channel coefficient outside 0..255");
}
std::vector<uint8_t> readFile(const std::filesystem::path &p) {
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  if (!f)
    throw std::runtime_error("Cannot open " + p.string());
  auto n = f.tellg();
  if (n <= 0 || n > 64 * 1024 * 1024)
    throw std::runtime_error("Empty file or input exceeds 64 MiB limit");
  std::vector<uint8_t> d(static_cast<size_t>(n));
  f.seekg(0);
  if (!f.read(reinterpret_cast<char *>(d.data()), n))
    throw std::runtime_error("Cannot read input");
  return d;
}
namespace {
constexpr int ayLevels[] = {0,     836,   1212,  1773,  2619,  3875,
                            5397,  8823,  10392, 16706, 23339, 29292,
                            36969, 46421, 55195, 65535};
constexpr int ymLevels[] = {
    0,      0,      0xf8,   0x1c2,  0x29e,  0x33a,  0x3f2,  0x4d7,
    0x610,  0x77f,  0x90a,  0xa42,  0xc3b,  0xec2,  0x1137, 0x13a7,
    0x1750, 0x1bf9, 0x20df, 0x2596, 0x2c9d, 0x3579, 0x3e55, 0x4768,
    0x54ff, 0x6624, 0x773b, 0x883f, 0xa1da, 0xc0fc, 0xe094, 0xffff};
struct Chip {
  std::array<uint8_t, 14> regs{};
  std::array<uint16_t, 3> count{};
  std::array<int, 3> tone{};
  uint16_t noiseCount = 0;
  uint32_t seed = 0xffff, envCount = 0;
  int amp = 0;
  bool first = false;
  void write(int r, int v) {
    regs.at(r) = uint8_t(v);
    if (r == 13) {
      envCount = 0;
      first = true;
      amp = (v & 4) ? -1 : 32;
    }
  }
  int word(int r) const { return regs[r] | (regs[r + 1] << 8); }
  void envelope() {
    int shape = regs[13];
    if (shape <= 3 || shape == 9) {
      if (first && --amp == 0)
        first = false;
    } else if ((shape >= 4 && shape <= 7) || shape == 15) {
      if (first && ++amp == 32) {
        first = false;
        amp = 0;
      }
    } else if (shape == 8)
      amp = (amp - 1) & 31;
    else if (shape == 10) {
      if (first) {
        if (--amp < 0) {
          first = false;
          amp = 0;
        }
      } else if (++amp == 32) {
        first = true;
        amp = 31;
      }
    } else if (shape == 11) {
      if (first && --amp < 0) {
        first = false;
        amp = 31;
      }
    } else if (shape == 12)
      amp = (amp + 1) & 31;
    else if (shape == 13) {
      if (first && ++amp == 32) {
        first = false;
        amp = 31;
      }
    } else if (shape == 14) {
      if (!first) {
        if (--amp < 0) {
          first = true;
          amp = 0;
        }
      } else if (++amp == 32) {
        first = false;
        amp = 31;
      }
    }
  }
  void tick() {
    for (int i = 0; i < 3; ++i) {
      ++count[i];
      if (count[i] >= word(i * 2)) {
        count[i] = 0;
        tone[i] ^= 1;
      }
    }
    ++noiseCount;
    if (!(noiseCount & 1) && noiseCount >= (regs[6] << 1)) {
      noiseCount = 0;
      seed =
          (((seed << 1) | 1) ^ ((seed >> 16) ^ ((seed >> 13) & 1))) & 0x1ffff;
    }
    if (!envCount)
      envelope();
    if (++envCount >= uint32_t(word(11)))
      envCount = 0;
  }
};
struct Synth {
  Profile p;
  std::array<Chip, 2> chips;
  int chipCount = 1;
  std::array<std::array<int, 32>, 6> levels{};
  std::vector<int> coefficients;
  std::array<std::vector<int>, 2> history;
  int index = 0, mode = -1;
  int64_t tickCounter = 0, nextSample = 0, step = 0;
  std::array<int, 2> previous{}, current{};
  std::array<int64_t, 2> sums{};
  std::array<int, 6> voicePrevious{}, voiceCurrent{};
  std::array<int64_t, 6> voiceSums{};
  explicit Synth(Profile pr, int count = 1) : p(pr), chipCount(count) {
    p.validate();
    step = int64_t(std::nearbyint(8192.0 / p.rate * p.clock));
    nextSample = step;
    int l = p.gains[0] + p.gains[2] + p.gains[4],
        r = p.gains[1] + p.gains[3] + p.gains[5];
    int norm = std::max({1, l + p.dmaGain, r + p.dmaGain, l * 2, r * 2});
    for (int c = 0; c < 6; ++c)
      for (int i = 0; i < 32; ++i)
        levels[c][i] = int(p.gains[c] / double(norm) *
                               (p.ym ? ymLevels[i] : ayLevels[i / 2]) /
                               65535.0 * 32767 * (p.preamp / 255.0 * 2) +
                           0.5);
    if (p.filter && p.rate < p.clock / 8) {
      double cutoff = 22050;
      mode = 0;
      if (p.rate >= 44100) {
        cutoff = p.rate / 2.0;
        mode = 1;
      }
      int taps = int(std::nearbyint(3.3 / (cutoff - 9200) * (p.clock / 8)));
      if (int64_t(p.clock) * taps > 3500000 * 50) {
        taps = int(std::nearbyint(3500000.0 * 50 / p.clock));
        mode = 0;
      }
      taps = std::max(1, taps);
      double c = std::numbers::pi * (9200 + cutoff) / (p.clock / 8), sum = 0,
             center = (taps - 1) / 2.0;
      std::vector<double> raw;
      for (int i = 0; i < taps; ++i) {
        double x = i - center;
        double f =
            x == 0
                ? c
                : std::sin(c * x) / x *
                      (0.54 + 0.46 * std::cos(2 * std::numbers::pi / taps * x));
        raw.push_back(f);
        sum += f;
      }
      for (double f : raw)
        coefficients.push_back(int(std::nearbyint(f / sum * 16777216)));
      for (auto &h : history)
        h.resize(taps);
    }
  }
  int filter(int lev, int ch, int &idx) {
    auto &h = history[ch];
    h[idx] = lev;
    int64_t value = int64_t(lev) * coefficients[0];
    for (size_t j = 1; j < coefficients.size(); ++j) {
      if (idx > 0)
        --idx;
      else
        idx = int(coefficients.size()) - 1;
      value += int64_t(h[idx]) * coefficients[j];
    }
    return int(value / 16777216);
  }
  void ticks(int n, std::vector<int16_t> &out,
             std::vector<std::array<int16_t, 6>> *voices = nullptr) {
    for (int t = 0; t < n; ++t) {
      if (tickCounter >= nextSample) {
        do {
          int ofs = int(nextSample - tickCounter + 65536);
          for (int c = 0; c < 2; ++c) {
            int64_t value =
                mode > 0 ? (int64_t(current[c] - previous[c]) * ofs / 65536 +
                            previous[c])
                         : sums[c] / (tickCounter >> 16);
            out.push_back(int16_t(std::clamp<int64_t>(value, -32768, 32767)));
          }
          if (voices) {
            std::array<int16_t, 6> sample{};
            for (int v = 0; v < chipCount * 3; ++v) {
              int64_t value =
                  mode > 0 ? (int64_t(voiceCurrent[v] - voicePrevious[v]) *
                                  ofs / 65536 +
                              voicePrevious[v])
                           : voiceSums[v] / (tickCounter >> 16);
              sample[v] = int16_t(std::clamp<int64_t>(value, 0, 32767));
            }
            voices->push_back(sample);
          }
          nextSample += step;
        } while (tickCounter >= nextSample);
        nextSample -= tickCounter;
        tickCounter = 0;
        sums = {};
        voiceSums = {};
      }
      std::array<int, 2> mixed{};
      std::array<int, 6> rawVoices{};
      for (int n = 0; n < chipCount; ++n) {
        auto &chip = chips[n];
        chip.tick();
        for (int c = 0; c < 3; ++c) {
          int k = (chip.regs[7] & (1 << c)) ? 1 : chip.tone[c];
          if (!(chip.regs[7] & (8 << c)))
            k &= int(chip.seed >> 16);
          if (k) {
            int amp =
                (chip.regs[8 + c] & 16) ? chip.amp : chip.regs[8 + c] * 2 + 1;
            if (amp < 0 || amp > 31)
              throw std::runtime_error("Invalid envelope amplitude");
            if (voices)
              rawVoices[n * 3 + c] =
                  int(int64_t(p.ym ? ymLevels[amp] : ayLevels[amp / 2]) *
                      32767 / 65535);
            mixed[0] += levels[c * 2][amp];
            mixed[1] += levels[c * 2 + 1][amp];
          }
        }
      }
      if (mode >= 0) {
        int idx = index;
        mixed[0] = filter(mixed[0], 0, index);
        mixed[1] = filter(mixed[1], 1, idx);
        index = idx;
      }
      if (voices) {
        voicePrevious = voiceCurrent;
        voiceCurrent = rawVoices;
        for (int v = 0; v < 6; ++v)
          voiceSums[v] += rawVoices[v];
      }
      previous = current;
      current = mixed;
      for (int c = 0; c < 2; ++c)
        sums[c] += mixed[c];
      tickCounter += 65536;
    }
  }
};
} // namespace
struct StreamRenderer::Impl {
  Profile profile;
  Song song;
  std::unique_ptr<Pt3> player, second;
  std::shared_ptr<const LogDecoder> log;
  std::unique_ptr<Synth> synth;
  uint64_t interrupt = 0, framePosition = 0, frames = 0;
  int ticks = 0, chipCount = 1;
  bool trace = false, capture = false;
  AudioChunk pending;
  size_t offset = 0;
  Impl(const std::filesystem::path &path, Profile requested, bool events,
       bool voices)
      : profile(requested), trace(events), capture(voices) {
    auto data = readFile(path);
    if (data.size() >= 16 &&
        std::string(data.begin(), data.begin() + 4) ==
            std::string("PSG\x1a", 4) &&
        data[4] == 10 && data[5] && profile.useFileTiming)
      profile.interruptHz = data[5];
    profile.validate();
    bool pt3 = data.size() >= 10 &&
               (std::string(data.begin(), data.begin() + 10) == "ProTracker" ||
                std::string(data.begin(), data.begin() + 6) == "Vortex");
    if (pt3) {
      player = std::make_unique<Pt3>(data);
      bool turbo = data.size() > 202 && data[13] >= '7' && data[13] <= '9' &&
                   data[98] != 32;
      song = player->song;
      song.interrupts = pt3Duration(*player, turbo);
      if (turbo) {
        chipCount = 2;
        second = std::make_unique<Pt3>(data, true);
        player->partner = second.get();
        second->partner = player.get();
        player->allowPatternLoop = second->allowPatternLoop = true;
        song.format = "PT3.7 TurboSound";
      }
    } else {
      log = std::make_shared<LogDecoder>(data, path.stem().string());
      song = log->song;
    }
    if (song.interrupts > uint64_t(profile.interruptHz * profile.maxSeconds))
      throw std::runtime_error(
          "Render exceeds configured duration budget (--max-seconds)");
    synth = std::make_unique<Synth>(profile, chipCount);
    ticks = int(std::nearbyint(profile.clock / (profile.interruptHz * 8)));
    const uint64_t totalTicks = song.interrupts * uint64_t(ticks);
    frames = totalTicks ? (totalTicks - 1) * 65536 / uint64_t(synth->step) : 0;
  }
  Impl(const Impl &other)
      : profile(other.profile), song(other.song), log(other.log),
        synth(std::make_unique<Synth>(*other.synth)),
        interrupt(other.interrupt), framePosition(other.framePosition),
        frames(other.frames), ticks(other.ticks), chipCount(other.chipCount),
        trace(other.trace), capture(other.capture), pending(other.pending),
        offset(other.offset) {
    if (other.player)
      player = std::make_unique<Pt3>(*other.player);
    if (other.second)
      second = std::make_unique<Pt3>(*other.second);
    if (second) {
      player->partner = second.get();
      second->partner = player.get();
    }
  }
  bool next() {
    if (interrupt >= song.interrupts)
      return false;
    pending = {};
    offset = 0;
    uint32_t ordinal = 0;
    auto apply = [&](const auto &writes, int chip) {
      for (auto [reg, value] : writes) {
        synth->chips[chip].write(reg, value);
        if (trace)
          pending.events.push_back({interrupt * uint64_t(ticks), ordinal++,
                                    uint8_t(chip), uint8_t(reg),
                                    uint8_t(value)});
      }
    };
    if (player) {
      if (!player->frame())
        throw std::runtime_error("PT3 ended before duration scan");
      if (second)
        second->frame();
      apply(player->writes, 0);
      if (second)
        apply(second->writes, 1);
    } else
      apply(log->frames[interrupt], 0);
    synth->ticks(ticks, pending.pcm, capture ? &pending.voices : nullptr);
    ++interrupt;
    return true;
  }
};
StreamRenderer::StreamRenderer(const std::filesystem::path &path,
                               const Profile &profile, bool trace, bool voices)
    : m(std::make_unique<Impl>(path, profile, trace, voices)) {}
StreamRenderer::StreamRenderer(const StreamRenderer &other)
    : m(std::make_unique<Impl>(*other.m)) {}
StreamRenderer &StreamRenderer::operator=(const StreamRenderer &other) {
  if (this != &other)
    m = std::make_unique<Impl>(*other.m);
  return *this;
}
StreamRenderer::StreamRenderer(StreamRenderer &&) noexcept = default;
StreamRenderer &StreamRenderer::operator=(StreamRenderer &&) noexcept = default;
StreamRenderer::~StreamRenderer() = default;
const Song &StreamRenderer::song() const { return m->song; }
const Profile &StreamRenderer::profile() const { return m->profile; }
uint64_t StreamRenderer::totalFrames() const { return m->frames; }
uint64_t StreamRenderer::position() const { return m->framePosition; }
int StreamRenderer::voiceCount() const { return m->chipCount * 3; }
AudioChunk StreamRenderer::read(size_t count) {
  AudioChunk out;
  out.startFrame = m->framePosition;
  count = std::min<uint64_t>(count, m->frames - m->framePosition);
  out.pcm.reserve(count * 2);
  if (m->capture)
    out.voices.reserve(count);
  while (out.pcm.size() / 2 < count) {
    if (m->offset >= m->pending.pcm.size() / 2) {
      if (!m->next())
        break;
    }
    out.events.insert(out.events.end(), m->pending.events.begin(),
                      m->pending.events.end());
    m->pending.events.clear();
    size_t take = std::min(count - out.pcm.size() / 2,
                           m->pending.pcm.size() / 2 - m->offset);
    out.pcm.insert(out.pcm.end(), m->pending.pcm.begin() + m->offset * 2,
                   m->pending.pcm.begin() + (m->offset + take) * 2);
    if (m->capture)
      out.voices.insert(out.voices.end(), m->pending.voices.begin() + m->offset,
                        m->pending.voices.begin() + m->offset + take);
    m->offset += take;
    m->framePosition += take;
  }
  return out;
}
Render renderFile(const std::filesystem::path &path, const Profile &profile,
                  Progress progress, bool trace) {
  StreamRenderer engine(path, profile, trace);
  Render result;
  result.profile = engine.profile();
  result.song = engine.song();
  while (engine.position() < engine.totalFrames()) {
    auto chunk = engine.read(4096);
    if (result.pcm.size() * 2 + chunk.pcm.size() * 2 > profile.maxPcmBytes)
      throw std::runtime_error(
          "Render exceeds configured PCM memory budget (--memory-mib)");
    result.pcm.insert(result.pcm.end(), chunk.pcm.begin(), chunk.pcm.end());
    result.events.insert(result.events.end(), chunk.events.begin(),
                         chunk.events.end());
    if (progress && !progress(double(engine.position()) / engine.totalFrames()))
      throw std::runtime_error("Cancelled");
  }
  if (progress && !progress(1))
    throw std::runtime_error("Cancelled");
  return result;
}
void writePsg(const std::filesystem::path &path, const Render &r) {
  if (r.profile.interruptHz > 255 ||
      r.profile.interruptHz != std::floor(r.profile.interruptHz))
    throw std::runtime_error("PSG header cannot represent this interrupt rate; "
                             "choose an integer rate in 1..255 Hz");
  if (r.events.empty())
    throw std::runtime_error("PSG export requires an event-enabled render");
  bool dual = std::any_of(r.events.begin(), r.events.end(),
                          [](const Event &e) { return e.chip == 1; });
  if (dual) {
    auto secondName = path.stem();
    secondName += "-chip2";
    secondName += path.extension().native();
    auto second = path.parent_path() / secondName;
    if (std::filesystem::exists(path) || std::filesystem::exists(second))
      throw std::runtime_error(
          "One of the dual-chip PSG destinations already exists");
    Render part;
    part.song = r.song;
    part.profile = r.profile;
    for (int chip = 0; chip < 2; ++chip) {
      part.events.clear();
      for (auto e : r.events)
        if (e.chip == chip) {
          e.chip = 0;
          part.events.push_back(e);
        }
      try {
        writePsg(chip ? second : path, part);
      } catch (...) {
        if (chip)
          std::filesystem::remove(path);
        throw;
      }
    }
    return;
  }

  if (std::filesystem::exists(path))
    throw std::runtime_error("Output exists; choose a new filename");
  auto temporary = path;
  temporary += ".ayplayer-part";
  if (std::filesystem::exists(temporary))
    throw std::runtime_error("Temporary output exists");
  try {
    std::ofstream f(temporary, std::ios::binary);
    if (!f)
      throw std::runtime_error("Cannot create PSG export");
    f.write("PSG\x1a", 4);
    f.put(10);
    f.put(char(int(r.profile.interruptHz)));
    for (int i = 0; i < 10; ++i)
      f.put(0);
    size_t event = 0;
    uint64_t ticks =
        uint64_t(std::nearbyint(r.profile.clock / (r.profile.interruptHz * 8)));
    for (uint64_t frame = 0; frame < r.song.interrupts; ++frame) {
      while (event < r.events.size() && r.events[event].tick == frame * ticks) {
        const auto &e = r.events[event++];
        if (e.chip)
          throw std::runtime_error(
              "Single PSG cannot represent multiple chips");
        f.put(char(e.reg));
        f.put(char(e.value));
      }
      f.put(char(255));
    }
    f.flush();
    if (!f)
      throw std::runtime_error("PSG write failed");
    f.close();
    commitExport(temporary, path);
  } catch (...) {
    std::filesystem::remove(temporary);
    throw;
  }
}

void writeWav(const std::filesystem::path &path, const Render &r) {
  if (r.pcm.size() > UINT32_MAX / 2 - 36)
    throw std::runtime_error("WAV too large");
  auto temporary = path;
  temporary += ".ayplayer-part";
  if (std::filesystem::exists(temporary))
    throw std::runtime_error("Temporary output already exists");
  try {
    std::ofstream f(temporary, std::ios::binary);
    if (!f)
      throw std::runtime_error("Cannot create export");
    auto u16 = [&](uint16_t v) {
      f.put(char(v & 255));
      f.put(char(v >> 8));
    };
    auto u32 = [&](uint32_t v) {
      u16(v & 65535);
      u16(v >> 16);
    };
    f.write("RIFF", 4);
    u32(uint32_t(r.pcm.size() * 2 + 36));
    f.write("WAVEfmt ", 8);
    u32(16);
    u16(1);
    u16(2);
    u32(r.profile.rate);
    u32(r.profile.rate * 4);
    u16(4);
    u16(16);
    f.write("data", 4);
    u32(uint32_t(r.pcm.size() * 2));
    for (int16_t v : r.pcm)
      u16(uint16_t(v));
    f.flush();
    if (!f)
      throw std::runtime_error("Export write failed");
    f.close();
    commitExport(temporary, path);
  } catch (...) {
    std::filesystem::remove(temporary);
    throw;
  }
}
} // namespace ay
