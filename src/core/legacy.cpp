// PT2/STC routines ported from Sergey Bulba's AY_Emul 3.0 beta Players.pas.
#include "legacy.h"
#include "tables.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
namespace ay {
namespace {
constexpr std::array<int, 96> stNotes = {
    0xef8, 0xe10, 0xd60, 0xc80, 0xbd8, 0xb28, 0xa88, 0x9f0, 0x960, 0x8e0, 0x858,
    0x7e0, 0x77c, 0x708, 0x6b0, 0x640, 0x5ec, 0x594, 0x544, 0x4f8, 0x4b0, 0x470,
    0x42c, 0x3f0, 0x3be, 0x384, 0x358, 0x320, 0x2f6, 0x2ca, 0x2a2, 0x27c, 0x258,
    0x238, 0x216, 0x1f8, 0x1df, 0x1c2, 0x1ac, 0x190, 0x17b, 0x165, 0x151, 0x13e,
    0x12c, 0x11c, 0x10b, 0xfc,  0xef,  0xe1,  0xd6,  0xc8,  0xbd,  0xb2,  0xa8,
    0x9f,  0x96,  0x8e,  0x85,  0x7e,  0x77,  0x70,  0x6b,  0x64,  0x5e,  0x59,
    0x54,  0x4f,  0x4b,  0x47,  0x42,  0x3f,  0x3b,  0x38,  0x35,  0x32,  0x2f,
    0x2c,  0x2a,  0x27,  0x25,  0x23,  0x21,  0x1f,  0x1d,  0x1c,  0x1a,  0x19,
    0x17,  0x16,  0x15,  0x13,  0x12,  0x11,  0x10,  0xf};
std::string title(const std::vector<uint8_t> &d, size_t a, size_t n) {
  std::string s(d.begin() + a, d.begin() + a + n);
  s.resize(s.find('\0') == std::string::npos ? s.size() : s.find('\0'));
  while (!s.empty() && s.back() == ' ')
    s.pop_back();
  return s;
}
} // namespace
int Legacy::byte(size_t p) const {
  if (p >= data.size())
    throw std::runtime_error(song.format + " read outside module at " +
                             std::to_string(p));
  return data[p];
}
Legacy::Legacy(std::vector<uint8_t> d, bool soundTracker)
    : data(std::move(d)), stc(soundTracker) {
  song.format = stc ? "STC" : "PT2";
  song.version = stc ? 1 : 2;
  if (data.size() < (stc ? 27u : 132u) || data.size() > 65536)
    throw std::runtime_error("Invalid " + song.format + " size");
  delay = byte(0);
  if (!delay)
    throw std::runtime_error("Zero tracker tempo");
  if (stc) {
    positions = byte(word(1)) + 1;
    song.title = title(data, 7, 18);
    if (song.title.starts_with("SONG BY ST COMPILE"))
      song.title.clear();
    byte(word(1) + positions * 2);
    for (auto &c : channels)
      c.ornament = word(3) + 1;
  } else {
    positions = byte(1);
    if (!positions || byte(2) >= positions)
      throw std::runtime_error("Invalid PT2 positions/loop");
    byte(131 + positions);
    song.title = title(data, 101, 30);
    for (auto &c : channels) {
      sample(c, 1);
      ornament(c, 0);
    }
  }
  patterns();
}
void Legacy::patterns() {
  size_t p;
  if (stc) {
    transpose = byte(word(1) + 2 + position * 2);
    int id = byte(word(1) + 1 + position * 2);
    p = word(5);
    int budget = 256;
    while (byte(p) != id) {
      if (byte(p) == 255 || !--budget)
        throw std::runtime_error("STC pattern not found");
      p += 7;
    }
    ++p;
  } else
    p = word(99) + byte(131 + position) * 6;
  for (int i = 0; i < 3; ++i) {
    channels[i].address = word(p + 2 * i);
    byte(channels[i].address);
  }
}
void Legacy::sample(Channel &c, int n) {
  if (stc) {
    size_t p = 27;
    int budget = 256;
    while (byte(p) != n) {
      if (!--budget)
        throw std::runtime_error("STC sample not found");
      p += 99;
    }
    c.sample = p + 1;
    byte(c.sample + 97);
  } else {
    size_t p = word(3 + 2 * n);
    c.smpLength = byte(p);
    c.smpLoop = byte(p + 1);
    c.sample = p + 2;
    if (!c.smpLength)
      throw std::runtime_error("Empty PT2 sample");
    byte(c.sample + c.smpLength * 3 - 1);
  }
}
void Legacy::ornament(Channel &c, int n) {
  if (stc) {
    size_t p = word(3);
    int budget = 256;
    while (byte(p) != n) {
      if (!--budget)
        throw std::runtime_error("STC ornament not found");
      p += 33;
    }
    c.ornament = p + 1;
    byte(c.ornament + 31);
    c.envelope = false;
  } else {
    size_t p = word(67 + 2 * n);
    c.ornLength = byte(p);
    c.ornLoop = byte(p + 1);
    c.ornament = p + 2;
    c.ornPos = 0;
    if (!c.ornLength)
      throw std::runtime_error("Empty PT2 ornament");
    byte(c.ornament + c.ornLength - 1);
  }
}
void Legacy::interpret(Channel &c) {
  bool done = false, gliss = false;
  int budget = 65536;
  auto next = [&]() { return byte(c.address++); };
  while (!done) {
    if (!--budget)
      throw std::runtime_error("Nonterminating " + song.format + " row");
    int b = next();
    if (stc) {
      if (b < 0x60) {
        c.note = b;
        c.sampleTicks = 32;
        c.smpPos = 0;
        done = true;
      } else if (b < 0x70)
        sample(c, b - 0x60);
      else if (b < 0x80)
        ornament(c, b - 0x70);
      else if (b == 0x80) {
        c.sampleTicks = -1;
        done = true;
      } else if (b == 0x81)
        done = true;
      else if (b == 0x82)
        ornament(c, 0);
      else if (b <= 0x8e) {
        write(13, b - 0x80);
        regs[11] = next();
        ornament(c, 0);
        c.envelope = true;
      } else
        c.skip = uint8_t(b - 0xa1);
    } else {
      if (b >= 0xe1)
        sample(c, b - 0xe0);
      else if (b == 0xe0) {
        c.smpPos = c.ornPos = 0;
        c.slide = 0;
        c.glissType = 0;
        c.enabled = false;
        done = true;
      } else if (b >= 0x80) {
        c.smpPos = c.ornPos = 0;
        c.slide = 0;
        if (gliss) {
          c.target = b - 0x80;
          if (c.glissType == 1)
            c.note = c.target;
        } else {
          c.note = b - 0x80;
          c.glissType = 0;
        }
        c.enabled = true;
        done = true;
      } else if (b == 0x7f)
        c.envelope = false;
      else if (b >= 0x71) {
        write(13, b - 0x70);
        regs[11] = next();
        regs[12] = next();
        c.envelope = true;
      } else if (b == 0x70)
        done = true;
      else if (b >= 0x60)
        ornament(c, b - 0x60);
      else if (b >= 0x20)
        c.skip = b - 0x20;
      else if (b >= 0x10)
        c.volume = b - 0x10;
      else if (b == 0x0f) {
        delay = next();
        if (!delay)
          throw std::runtime_error("Zero PT2 tempo");
      } else if (b == 0x0e) {
        c.gliss = s8(next());
        c.glissType = 1;
        gliss = true;
      } else if (b == 0x0d) {
        c.gliss = s8(std::abs(s8(next())));
        next();
        next();
        c.glissType = 2;
        gliss = true;
      } else if (b == 0x0c)
        c.glissType = 0;
      else
        c.noise = s8(next());
    }
  }
  if (gliss && c.glissType == 2) {
    c.delta = std::abs(PT3NoteTable_ST[c.target] - PT3NoteTable_ST[c.note]);
    if (c.target > c.note)
      c.gliss = s8(-c.gliss);
  }
  c.count = s8(c.skip);
}
void Legacy::change(Channel &c, uint8_t &mixer) {
  if (stc) {
    if (c.sampleTicks >= 0) {
      c.sampleTicks = s8(c.sampleTicks - 1);
      c.smpPos = (c.smpPos + 1) & 31;
      if (c.sampleTicks == 0) {
        int loop = byte(c.sample + 96);
        if (loop) {
          c.smpPos = loop & 31;
          c.sampleTicks = s8(byte(c.sample + 97) + 1);
        } else
          c.sampleTicks = -1;
      }
    }
    if (c.sampleTicks >= 0) {
      int index = (c.smpPos - 1) & 31;
      size_t p = c.sample + index * 3;
      int b0 = byte(p), b1 = byte(p + 1);
      if (b1 & 0x80)
        mixer |= 64;
      else
        regs[6] = b1 & 31;
      if (b1 & 0x40)
        mixer |= 8;
      int n = std::min(
          int(uint8_t(c.note + byte(c.ornament + index) + transpose)), 95);
      int off = byte(p + 2) + ((b0 & 0xf0) << 4);
      c.tone = (stNotes[n] + ((b1 & 0x20) ? off : -off)) & 4095;
      c.amp = (b0 & 15) | (c.envelope ? 16 : 0);
    } else
      c.amp = 0;
  } else if (c.enabled) {
    size_t p = c.sample + c.smpPos * 3;
    int b0 = byte(p), b1 = byte(p + 1), off = byte(p + 2) + ((b1 & 15) << 8);
    if (!(b0 & 4))
      off = -off;
    int n = uint8_t(c.note + byte(c.ornament + c.ornPos));
    n = s8(n) < 0 ? 0 : std::min(n, 95);
    c.tone = (off + c.slide + PT3NoteTable_ST[n]) & 4095;
    if (c.glissType == 2) {
      c.delta = s16(c.delta - std::abs(c.gliss));
      if (c.delta < 0) {
        c.note = c.target;
        c.glissType = 0;
        c.slide = 0;
      }
    }
    if (c.glissType)
      c.slide = s16(c.slide + c.gliss);
    c.amp = int(std::nearbyint((c.volume * 17 + (c.volume > 7)) * (b1 >> 4) /
                               256.0)) |
            (c.envelope ? 16 : 0);
    if (b0 & 1)
      mixer |= 64;
    else
      regs[6] = ((b0 >> 3) + uint8_t(c.noise)) & 31;
    if (b0 & 2)
      mixer |= 8;
    c.smpPos = uint8_t(c.smpPos + 1);
    if (c.smpPos == c.smpLength)
      c.smpPos = c.smpLoop;
    c.ornPos = uint8_t(c.ornPos + 1);
    if (c.ornPos == c.ornLength)
      c.ornPos = c.ornLoop;
  } else
    c.amp = 0;
  mixer >>= 1;
}
bool Legacy::frame() {
  writes.clear();
  counter = uint8_t(counter - 1);
  if (!counter) {
    for (int i = 0; i < 3; ++i) {
      auto &c = channels[i];
      c.count = s8(c.count - 1);
      if (c.count < 0) {
        if (i == 0 && byte(c.address) == (stc ? 255 : 0)) {
          if (++position >= positions)
            return false;
          patterns();
        }
        interpret(c);
      }
    }
    counter = delay;
  }
  uint8_t mixer = 0;
  for (auto &c : channels)
    change(c, mixer);
  write(7, mixer);
  for (int i = 0; i < 3; ++i) {
    write(2 * i, channels[i].tone & 255);
    write(2 * i + 1, channels[i].tone >> 8);
  }
  for (int i = 0; i < 3; ++i)
    write(8 + i, channels[i].amp);
  write(6, regs[6]);
  write(11, regs[11]);
  write(12, regs[12]);
  ++song.interrupts;
  return true;
}
uint64_t Legacy::duration(uint64_t budget) const {
  auto scan = *this;
  uint64_t n = 0;
  while (scan.frame()) {
    if (++n >= budget)
      throw std::runtime_error("Tracker duration exceeds scan budget");
  }
  return n;
}
void convertToPt3(const std::filesystem::path &input,
                  const std::filesystem::path &output) {
  auto ext = input.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (ext != ".pt2" && ext != ".stc")
    throw std::runtime_error("convert-pt3 accepts PT2 or STC modules");
  if (std::filesystem::exists(output))
    throw std::runtime_error("PT3 destination already exists");
  Legacy source(readFile(input), ext == ".stc");
  struct Tick {
    std::array<uint8_t, 14> regs;
    std::vector<int> shapes;
    std::array<int, 3> notes, points;
    int position = 0;
  };
  std::vector<Tick> ticks;
  std::map<std::array<uint8_t, 4>, int> ids;
  std::vector<std::array<uint8_t, 4>> points;
  while (source.frame()) {
    if (ticks.size() >= 300000)
      throw std::runtime_error("Conversion duration exceeds scan budget");
    Tick t{};
    t.regs = source.regs;
    t.position = source.position;
    for (auto [r, v] : source.writes)
      if (r == 13)
        t.shapes.push_back(v);
    for (int ch = 0; ch < 3; ++ch) {
      int tone = t.regs[2 * ch] | (t.regs[2 * ch + 1] << 8), note = 0;
      for (int n = 1; n < 96; ++n)
        if (std::abs(tone - int(PT3NoteTable_ST[n])) <
            std::abs(tone - int(PT3NoteTable_ST[note])))
          note = n;
      int off = uint16_t(tone - PT3NoteTable_ST[note]);
      std::array<uint8_t, 4> point = {
          uint8_t(!(t.regs[8 + ch] & 16)),
          uint8_t((t.regs[8 + ch] & 15) | ((t.regs[7] & (1 << ch)) ? 16 : 0) |
                  ((t.regs[7] & (8 << ch)) ? 128 : 0)),
          uint8_t(off), uint8_t(off >> 8)};
      auto [it, added] = ids.emplace(point, int(points.size()));
      if (added)
        points.push_back(point);
      t.notes[ch] = note;
      t.points[ch] = it->second;
    }
    ticks.push_back(t);
  }
  if (ticks.empty())
    throw std::runtime_error("Cannot convert an empty module");
  if (points.size() > 31 * 255)
    throw std::runtime_error("Conversion exceeds PT3's 31 sample banks");
  std::vector<uint8_t> d(202, 32);
  auto put = [&](size_t p, int value) {
    d.at(p) = uint8_t(value);
    d.at(p + 1) = uint8_t(value >> 8);
  };
  std::vector<uint8_t> *active = &d;
  auto append = [&](int value) {
    if (active->size() >= 65536)
      throw std::runtime_error(
          "Lossless conversion exceeds PT3's 64 KiB limit");
    active->push_back(uint8_t(value));
  };
  std::string header = "ProTracker 3.6 compilation of ";
  std::copy(header.begin(), header.end(), d.begin());
  std::fill(d.begin() + 30, d.begin() + 98, 32);
  auto name =
      source.song.title.empty() ? input.stem().string() : source.song.title;
  std::copy_n(name.begin(), std::min(size_t(32), name.size()), d.begin() + 30);
  std::fill(d.begin() + 62, d.begin() + 66, 32);
  d[98] = 32;
  d[99] = 1;
  d[100] = 1;
  d[101] = 1;
  d[102] = 0;
  d[201] = 0;

  std::vector<int> order(points.size()), frequency(points.size());
  for (auto &tick : ticks)
    for (int point : tick.points)
      ++frequency[point];
  for (size_t i = 0; i < order.size(); ++i)
    order[i] = i;
  std::stable_sort(order.begin(), order.end(),
                   [&](int a, int b) { return frequency[a] > frequency[b]; });
  int singles =
      std::min(int(points.size()), 31 - int((points.size() + 254) / 255));
  std::vector<std::vector<int>> samples;
  for (int i = 0; i < singles; ++i)
    samples.push_back({order[i]});
  for (size_t i = singles; i < order.size(); i += 255)
    samples.emplace_back(order.begin() + i,
                         order.begin() + std::min(order.size(), i + 255));
  std::vector<std::pair<int, int>> location(points.size());
  for (size_t bank = 0; bank < samples.size(); ++bank) {
    put(105 + 2 * (bank + 1), d.size());
    append(0);
    int length = samples[bank].size();
    append(length);
    for (int i = 0; i < length; ++i) {
      location[samples[bank][i]] = {bank + 1, i};
      for (auto b : points[samples[bank][i]])
        append(b);
    }
  }
  size_t orn = d.size();
  append(0);
  append(1);
  append(0);
  for (int i = 0; i < 16; ++i)
    put(169 + 2 * i, orn);
  // Keep envelope enabled in all voices; the sample's envelope mask selects it
  // per tick. Initial same-tick retriggers cannot advance the chip envelope.
  std::vector<size_t> boundaries{0};
  for (size_t i = 1; i < ticks.size(); ++i)
    if (ticks[i].position != ticks[i - 1].position)
      boundaries.push_back(i);
  boundaries.push_back(ticks.size());
  using Pattern = std::array<std::vector<uint8_t>, 3>;
  std::vector<Pattern> patterns;
  struct PatternLess {
    bool operator()(const Pattern &a, const Pattern &b) const {
      for (int ch = 0; ch < 3; ++ch) {
        if (a[ch].size() != b[ch].size())
          return a[ch].size() < b[ch].size();
        for (size_t i = 0; i < a[ch].size(); ++i)
          if (a[ch][i] != b[ch][i])
            return a[ch][i] < b[ch][i];
      }
      return false;
    }
  };
  std::map<Pattern, int, PatternLess> patternIds;
  std::vector<int> positions;
  for (size_t segment = 0; segment + 1 < boundaries.size(); ++segment) {
    Pattern pattern;
    size_t begin = boundaries[segment], end = boundaries[segment + 1];
    for (int ch = 0; ch < 3; ++ch) {
      active = &pattern[ch];
      int bank = 0, note = -1, pos = 0, skip = 0;
      for (size_t tick = begin; tick < end;) {
        auto &t = ticks[tick];
        int period = t.regs[11] | (t.regs[12] << 8);
        auto envelope = [&](int shape) {
          append(0xb1 + shape);
          append(period >> 8);
          append(period);
        };
        if (tick == 0) {
          append(0xb1);
          append(1);
          envelope(t.shapes.empty() ? 1 : t.shapes.back());
        }
        if (ch == 0) {
          for (int shape : t.shapes)
            envelope(shape);
          if (tick == begin || t.regs[6] != ticks[tick - 1].regs[6])
            append(0x20 + t.regs[6]);
        }
        auto [newBank, index] = location[t.points[ch]];
        int run = 1;
        if (samples[newBank - 1].size() == 1) {
          while (run < 127 && tick + run < end) {
            auto &next = ticks[tick + run];
            if (next.notes[ch] != t.notes[ch] ||
                next.points[ch] != t.points[ch] ||
                (ch == 0 &&
                 (!next.shapes.empty() || next.regs[6] != t.regs[6])))
              break;
            ++run;
          }
        }
        if (skip != run) {
          append(0xb1);
          append(run);
          skip = run;
        }
        bool newNote = note != t.notes[ch];
        if (bank != newBank) {
          append(0xd0 + newBank);
          bank = newBank;
        }
        if (newNote)
          pos = 0;
        if (pos != index)
          append(3);
        append(newNote ? 0x50 + t.notes[ch] : 0xd0);
        if (pos != index)
          append(index);
        note = t.notes[ch];
        pos = index + 1;
        if (pos >= int(samples[bank - 1].size()))
          pos = 0;
        tick += run;
      }
      append(0);
    }
    auto [it, added] = patternIds.emplace(pattern, int(patterns.size()));
    if (added)
      patterns.push_back(std::move(pattern));
    positions.push_back(it->second);
  }
  if (patterns.size() > 85 || positions.size() > 255)
    throw std::runtime_error("Conversion exceeds PT3 pattern/position limits");
  // Rebuild the small header with a deduplicated pattern sequence.
  auto instruments = d;
  std::vector<uint8_t> packed(d.begin(), d.begin() + 201);
  active = &packed;
  for (int id : positions)
    append(id * 3);
  append(255);
  size_t table = packed.size();
  for (size_t i = 0; i < patterns.size() * 6; ++i)
    append(0);
  size_t instrumentStart = packed.size();
  for (size_t i = 202; i < instruments.size(); ++i)
    append(instruments[i]);
  d = std::move(packed);
  active = &d;
  d[101] = positions.size();
  put(103, table);
  for (size_t bank = 0; bank < samples.size(); ++bank)
    put(105 + 2 * (bank + 1), (instruments[105 + 2 * (bank + 1)] |
                               (instruments[106 + 2 * (bank + 1)] << 8)) +
                                  instrumentStart - 202);
  for (int i = 0; i < 16; ++i)
    put(169 + 2 * i, orn + instrumentStart - 202);
  for (size_t pattern = 0; pattern < patterns.size(); ++pattern)
    for (int ch = 0; ch < 3; ++ch) {
      put(table + pattern * 6 + ch * 2, d.size());
      for (auto b : patterns[pattern][ch])
        append(b);
    }
  // Validate the real decoder before creating the destination. Exported PCM
  // uses the same tone/noise/mixer/amplitude/period state at every interrupt.
  Pt3 check(d);
  for (auto &t : ticks) {
    if (!check.frame())
      throw std::runtime_error("Converted PT3 ended early");
    for (int r = 0; r < 13; ++r)
      if (check.regs[r] != t.regs[r])
        throw std::runtime_error("PT3 conversion register verification failed");
  }
  if (check.frame())
    throw std::runtime_error("Converted PT3 has an unexpected tail");
  writeModuleFile(output, d);
}
} // namespace ay
