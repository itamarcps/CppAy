// Native C++ port of Sergey Bulba's AY_Emul PT3 player. See THIRD_PARTY.md.
#include "pt3.h"
#include "tables.h"
#include <algorithm>
#include <cctype>
namespace ay {
Pt3::Pt3(std::vector<uint8_t> d, bool mirror)
    : data(std::move(d)), mirrored(mirror) {
  if (data.size() < 202 || data.size() > 65536)
    throw std::runtime_error("Invalid PT3 size");
  if (std::string(data.begin(), data.begin() + 10) != "ProTracker" &&
      std::string(data.begin(), data.begin() + 6) != "Vortex")
    throw std::runtime_error("Unrecognized PT3 header");
  song.version = std::isdigit(byte(13)) ? byte(13) - '0' : 6;
  song.format = "PT3";
  song.title = std::string(data.begin() + 30, data.begin() + 62);
  song.author = std::string(data.begin() + 66, data.begin() + 98);
  for (auto *t : {&song.title, &song.author}) {
    auto n = t->find('\0');
    if (n != std::string::npos)
      t->resize(n);
    while (!t->empty() && t->back() == ' ')
      t->pop_back();
  }
  if (song.version >= 7 && byte(98) != 32 && (byte(98) < 1 || byte(98) > 85))
    throw std::runtime_error("Invalid PT3.7 TurboSound pattern count");
  delay = byte(100);
  if (!delay || !byte(101) || byte(102) >= byte(101))
    throw std::runtime_error("Invalid PT3 tempo/positions/loop");
  for (auto &c : channels) {
    setSample(c, 1);
    setOrnament(c, 0);
  }
  patterns();
}
void Pt3::patterns() {
  int p = byte(201 + position);
  if (mirrored)
    p = byte(98) * 3 - 3 - p;
  if (p % 3 || p > 252)
    throw std::runtime_error("Invalid PT3 pattern index");
  int a = word(103) + p * 2;
  for (int i = 0; i < 3; ++i)
    channels[i].address = word(a + i * 2);
}
void Pt3::setSample(Channel &c, int n) {
  if (n < 0 || n > 31)
    throw std::runtime_error("Invalid PT3 sample");
  int p = word(105 + n * 2);
  c.smpLoop = byte(p);
  c.smpLength = byte(p + 1);
  c.sample = p + 2;
  if (!c.smpLength || c.smpLoop >= c.smpLength)
    throw std::runtime_error("Invalid PT3 sample loop");
  byte(c.sample + c.smpLength * 4 - 1);
}
void Pt3::setOrnament(Channel &c, int n) {
  if (n < 0 || n > 15)
    throw std::runtime_error("Invalid PT3 ornament");
  int p = word(169 + n * 2);
  c.ornLoop = byte(p);
  c.ornLength = byte(p + 1);
  c.ornament = p + 2;
  if (!c.ornLength || c.ornLoop >= c.ornLength)
    throw std::runtime_error("Invalid PT3 ornament loop");
  byte(c.ornament + c.ornLength - 1);
}
int Pt3::frequency(int n) const {
  n = std::clamp(n, 0, 95);
  bool old = song.version <= 3;
  switch (byte(99)) {
  case 0:
    return (old ? PT3NoteTable_PT_33_34r : PT3NoteTable_PT_34_35)[n];
  case 1:
    return PT3NoteTable_ST[n];
  case 2:
    return (old ? PT3NoteTable_ASM_34r : PT3NoteTable_ASM_34_35)[n];
  default:
    return (old ? PT3NoteTable_REAL_34r : PT3NoteTable_REAL_34_35)[n];
  }
}
void Pt3::interpret(Channel &c) {
  int prevNote = c.note, prevSlide = c.slide, counter = 0;
  std::array<int, 10> flags{};
  bool quit = false;
  int budget = 65536;
  auto next = [&]() {
    int b = byte(c.address);
    ++c.address;
    return b;
  };
  auto reset = [&]() {
    c.smpPos = c.ornPos = c.noiseSlide = c.envSlide = 0;
    c.ampSlide = c.slideCount = c.slide = c.accum = c.onoff = 0;
  };
  auto envelope = [&](int shape) {
    write(13, shape);
    int hi = next();
    envBase = s16((hi << 8) | next());
    c.envelope = true;
    envSlide = 0;
    envCounter = 0;
  };
  while (!quit) {
    if (--budget == 0)
      throw std::runtime_error("Nonterminating PT3 row");
    int b = next();
    if (b >= 0xf0) {
      setOrnament(c, b - 0xf0);
      setSample(c, next() / 2);
      c.envelope = false;
      c.ornPos = 0;
    } else if (b >= 0xd1)
      setSample(c, b - 0xd0);
    else if (b == 0xd0)
      quit = true;
    else if (b >= 0xc1)
      c.volume = b - 0xc0;
    else if (b == 0xc0) {
      reset();
      c.enabled = false;
      quit = true;
    } else if (b >= 0xb2) {
      envelope(b - 0xb1);
      c.ornPos = 0;
    } else if (b == 0xb1)
      c.skip = next();
    else if (b == 0xb0) {
      c.envelope = false;
      c.ornPos = 0;
    } else if (b >= 0x50) {
      c.note = b - 0x50;
      reset();
      c.enabled = true;
      quit = true;
    } else if (b >= 0x40) {
      setOrnament(c, b - 0x40);
      c.ornPos = 0;
    } else if (b >= 0x20)
      noiseBase = b - 0x20;
    else if (b >= 0x10) {
      if (b == 0x10)
        c.envelope = false;
      else
        envelope(b - 0x10);
      setSample(c, next() / 2);
      c.ornPos = 0;
    } else if (b == 1 || b == 2 || b == 3 || b == 4 || b == 5 || b == 8 ||
               b == 9) {
      if (counter == 255)
        throw std::runtime_error("Too many PT3 row effects");
      flags[b] = ++counter;
    }
  }
  while (counter > 0) {
    if (counter == flags[1]) {
      c.slideDelay = next();
      c.slideCount = c.slideDelay;
      c.step = s16(word(c.address));
      c.address += 2;
      c.gliss = true;
      c.onoff = 0;
      if (!c.slideCount && song.version >= 7)
        ++c.slideCount;
    } else if (counter == flags[2]) {
      c.gliss = false;
      c.onoff = 0;
      c.slideDelay = next();
      c.slideCount = c.slideDelay;
      c.address += 2;
      c.step = s16(std::abs(s16(word(c.address))));
      c.address += 2;
      c.delta = s16(frequency(c.note) - frequency(prevNote));
      c.target = c.note;
      c.note = prevNote;
      if (song.version >= 6)
        c.slide = prevSlide;
      if (c.delta - c.slide < 0)
        c.step = s16(-c.step);
    } else if (counter == flags[3])
      c.smpPos = next();
    else if (counter == flags[4])
      c.ornPos = next();
    else if (counter == flags[5]) {
      c.onDelay = next();
      c.offDelay = next();
      c.onoff = c.onDelay;
      c.slideCount = c.slide = 0;
    } else if (counter == flags[8]) {
      envDelay = s8(next());
      envCounter = envDelay;
      envAdd = s16(word(c.address));
      c.address += 2;
    } else if (counter == flags[9]) {
      delay = next();
      if (!delay)
        throw std::runtime_error("Zero PT3 tempo");
      if (partner) {
        partner->delay = delay;
        if (mirrored)
          partner->delayCounter = delay;
      }
    }
    --counter;
  }
  c.skipCount = s8(c.skip);
}
void Pt3::change(Channel &c, uint8_t &mixer, int8_t &addEnv) {
  if (c.enabled) {
    int p = c.sample + c.smpPos * 4;
    c.tone = uint16_t(word(p + 2) + c.accum);
    int b0 = byte(p), b1 = byte(p + 1);
    if (b1 & 0x40)
      c.accum = s16(c.tone);
    uint8_t note = c.note + byte(c.ornament + c.ornPos);
    int n = s8(note) < 0 ? 0 : std::min(int(note), 95);
    c.tone = (c.tone + c.slide + frequency(n)) & 0xfff;
    if (c.slideCount > 0 && --c.slideCount == 0) {
      c.slide = s16(c.slide + c.step);
      c.slideCount = c.slideDelay;
      if (!c.gliss && ((c.step < 0 && c.slide <= c.delta) ||
                       (c.step >= 0 && c.slide >= c.delta))) {
        c.note = c.target;
        c.slideCount = c.slide = 0;
      }
    }
    c.amp = b1 & 15;
    if (b0 & 0x80) {
      if (b0 & 0x40) {
        if (c.ampSlide < 15)
          ++c.ampSlide;
      } else if (c.ampSlide > -15)
        --c.ampSlide;
    }
    c.amp = uint8_t(c.amp + c.ampSlide);
    if (s8(c.amp) < 0)
      c.amp = 0;
    else if (c.amp > 15)
      c.amp = 15;
    c.amp = (song.version <= 4 ? PT3VolumeTable_33_34
                               : PT3VolumeTable_35)[c.volume * 16 + c.amp];
    if (!(b0 & 1) && c.envelope)
      c.amp |= 16;
    if (b1 & 0x80) {
      uint8_t j =
          ((b0 & 0x20) ? ((b0 >> 1) | 0xf0) : ((b0 >> 1) & 15)) + c.envSlide;
      if (b1 & 0x20)
        c.envSlide = j;
      addEnv = s8(addEnv + j);
    } else {
      addNoise = uint8_t((b0 >> 1) + c.noiseSlide);
      if (b1 & 0x20)
        c.noiseSlide = addNoise;
    }
    mixer = ((b1 >> 1) & 0x48) | mixer;
    if (++c.smpPos >= c.smpLength)
      c.smpPos = c.smpLoop;
    if (++c.ornPos >= c.ornLength)
      c.ornPos = c.ornLoop;
  } else
    c.amp = 0;
  mixer >>= 1;
  if (c.onoff > 0 && --c.onoff == 0) {
    c.enabled = !c.enabled;
    c.onoff = c.enabled ? c.onDelay : c.offDelay;
  }
}
bool Pt3::frame() {
  writes.clear();
  delayCounter = uint8_t(delayCounter - 1);
  if (!delayCounter) {
    for (int i = 0; i < 3; ++i) {
      auto &c = channels[i];
      c.skipCount = s8(c.skipCount - 1);
      if (!c.skipCount) {
        if (i == 0 && byte(c.address) == 0) {
          if (++position >= byte(101)) {
            if (!allowPatternLoop)
              return false;
            position = byte(102);
          }
          patterns();
          noiseBase = 0;
        }
        interpret(c);
      }
    }
    delayCounter = delay;
  }
  uint8_t mixer = 0;
  int8_t addEnv = 0;
  for (auto &c : channels)
    change(c, mixer, addEnv);
  write(7, mixer);
  for (int i = 0; i < 3; ++i) {
    write(i * 2, channels[i].tone & 255);
    write(i * 2 + 1, channels[i].tone >> 8);
  }
  for (int i = 0; i < 3; ++i)
    write(8 + i, channels[i].amp);
  write(6, (noiseBase + addNoise) & 31);
  uint16_t e = uint16_t(envBase + addEnv + envSlide);
  write(11, e & 255);
  write(12, e >> 8);
  if (envCounter > 0 && --envCounter == 0) {
    envCounter = envDelay;
    envSlide = s16(envSlide + envAdd);
  }
  ++song.interrupts;
  return true;
}
} // namespace ay
namespace ay {
uint64_t pt3Duration(const Pt3 &p, bool turbo) {
  struct Scan {
    std::array<size_t, 3> address{};
    std::array<int, 3> count{1, 1, 1}, skip{1, 1, 1};
  };
  std::array<Scan, 2> scans;
  int tempo = p.byte(100);
  uint64_t duration = 0;
  int budget = 65536;
  auto row = [&](Scan &s) {
    for (int channel = 0; channel < 3; ++channel) {
      s.count[channel] = s8(s.count[channel] - 1);
      if (s.count[channel])
        continue;
      auto &addr = s.address[channel];
      if (channel == 0 && p.byte(addr) == 0)
        return false;
      int effects = 0;
      std::array<int, 9> flag{};
      int remaining = 65536;
      while (true) {
        if (--remaining == 0)
          throw std::runtime_error("Nonterminating PT3 duration scan");
        int b = p.byte(addr++);
        if (b == 0xd0 || b == 0xc0 || (b >= 0x50 && b <= 0xaf)) {
          s.count[channel] = s.skip[channel];
          break;
        }
        if (b == 0x10 || b >= 0xf0)
          ++addr;
        else if (b >= 0xb2 && b <= 0xbf)
          addr += 2;
        else if (b == 0xb1)
          s.skip[channel] = s8(p.byte(addr++));
        else if (b >= 0x11 && b <= 0x1f)
          addr += 3;
        else if (b == 1 || b == 2 || b == 3 || b == 4 || b == 5 || b == 8)
          flag[b] = ++effects;
        else if (b == 9)
          ++effects;
      }
      while (effects) {
        if (effects == flag[1] || effects == flag[8])
          addr += 3;
        else if (effects == flag[2])
          addr += 5;
        else if (effects == flag[3] || effects == flag[4])
          ++addr;
        else if (effects == flag[5])
          addr += 2;
        else {
          tempo = p.byte(addr++);
          if (!tempo)
            throw std::runtime_error("Zero PT3 duration tempo");
        }
        if (addr >= 65536)
          throw std::runtime_error("PT3 duration offset exceeds address space");
        --effects;
      }
    }
    return true;
  };
  for (int position = 0; position < p.byte(101); ++position) {
    for (int n = 0; n < (turbo ? 2 : 1); ++n) {
      int pattern = p.byte(201 + position);
      if (n)
        pattern = p.byte(98) * 3 - 3 - pattern;
      if (pattern < 0 || pattern > 252 || pattern % 3)
        throw std::runtime_error("Invalid PT3 duration pattern");
      for (int c = 0; c < 3; ++c) {
        scans[n].address[c] = p.word(p.word(103) + pattern * 2 + c * 2);
        scans[n].count[c] = 1;
      }
    }
    while (row(scans[0])) {
      if (turbo && !row(scans[1]))
        break;
      duration += tempo;
      if (--budget < 0)
        throw std::runtime_error("PT3 duration scan budget exceeded");
    }
  }
  return duration;
}
} // namespace ay
