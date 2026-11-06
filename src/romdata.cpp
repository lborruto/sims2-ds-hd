// The game's archive and strings, read from the player's ROM. The formats were worked out on
// the European game (ASJP) by reading its code; nothing of the game is in the repository.
// Every table read here comes from a file the player gave: each size and offset is checked
// before it is used, and a table that does not fit is refused, never guessed at.
// (GameLanguage, the only part that touches melonDS, is in romdata_nds.cpp so that
// tests/romdata_test.cpp links this file alone.)
#include "romdata.h"

#include <cstring>
#include <iterator>

namespace sims2wide
{
namespace
{
u16 Rd16(const u8* p) { return (u16)(p[0] | (p[1] << 8)); }
u32 Rd32(const u8* p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }

// The cartridge's file system (NitroFS). The ROM header gives the file name table (FNT: its
// offset at 0x40, its size at 0x44) and the file allocation table (FAT: 0x48 and 0x4C; u32
// start, u32 end per file id, absolute in the ROM). The FNT begins with the directories'
// entries, the root's first: the offset of its name list (from the FNT; past the root's own
// 8 bytes) and the id of its first file. The list has one entry per name: a length byte (bit
// 7: a directory, whose id follows the name; 0: the end), then the name; files take ids in
// order from the first. Finds a file of the root directory (the game keeps all of its there).
bool RootFile(const std::vector<u8>& rom, const char* name, u32& start, u32& end)
{
    const size_t size = rom.size();
    if (size < 0x50) return false;
    const u32 fnt = Rd32(&rom[0x40]), fntSize = Rd32(&rom[0x44]), fat = Rd32(&rom[0x48]), fatSize = Rd32(&rom[0x4C]);
    if (fntSize < 8 || fntSize > size || fnt > size - fntSize) return false;
    if (fatSize > size || fat > size - fatSize) return false;
    const u32 list = Rd32(&rom[fnt]);
    if (list < 8 || list >= fntSize) return false;
    const size_t fntEnd = (size_t)fnt + fntSize;
    size_t p = (size_t)fnt + list;
    u32 id = Rd16(&rom[fnt + 4]);
    const size_t nameLen = strlen(name);
    while (p < fntEnd)
    {
        const u8 type = rom[p++];
        if (type == 0) break;
        const u8 len = type & 0x7F;
        if (len > fntEnd - p) break;
        if (type & 0x80) { p += len + 2; continue; } // a directory: its id after the name
        if (len == nameLen && memcmp(&rom[p], name, len) == 0)
        {
            if (fatSize < 8 || id > (fatSize - 8) / 8) return false;
            const size_t entry = (size_t)fat + 8 * (size_t)id;
            start = Rd32(&rom[entry]);
            end = Rd32(&rom[entry + 4]);
            return start <= end && end <= size;
        }
        p += len;
        id++;
    }
    return false;
}

// The game's text is one byte a character: ASCII from the space to 'z' as is, the rest in the
// game's own order, worked out from the words of the French, German, Italian and Spanish
// tables (each accented byte read in the words around it): Latin-1's own order of the
// accented letters, less a few. '{' '|' '}' '~' (0x7B to 0x7E) hold the copyright sign, oe,
// the inverted exclamation and question marks; 0x7F to 0x99 the capitals A grave to U umlaut
// and the sharp s (Latin-1 0xC0 to 0xDF without Eth, the times sign, U circumflex, Y acute
// and Thorn); 0x9A to 0xB4 the small letters a grave to u umlaut (0xE0 to 0xFF without eth,
// the division sign, y acute, thorn and y umlaut); then the degree sign, an ellipsis, the
// registered sign and a no-break space. A slot marked "seen" was read in words of the
// languages named (EN, FR, DE, IT, ES; "a name": only in a Sim's name); one marked "order"
// sits between seen ones and follows Latin-1's order (0x95 and 0xB0 by the symmetry of the
// two runs, each one slot wide between O umlaut and U grave). 0xB6 was never seen, nor
// anything from 0xBA: they stay visible as {xx}. A byte of 0xF0 and over starts a 2-byte
// escape (the game's decoder: none in these five languages, only in the sixth table the game
// does not use), kept as {F0xx} until a meaning is found.
constexpr u16 kCharset[] = {
    0x00A9, // 7B (c)   seen: EN FR DE IT
    0x0153, // 7C oe    seen: FR
    0x00A1, // 7D !     seen: ES
    0x00BF, // 7E ?     seen: ES
    0x00C0, // 7F A`    seen: IT
    0x00C1, // 80 A'    seen: ES (a name elsewhere)
    0x00C2, // 81 A^    order
    0x00C3, // 82 A~    order
    0x00C4, // 83 A"    seen: DE
    0x00C5, // 84 A°    order
    0x00C6, // 85 AE    order
    0x00C7, // 86 C,    order
    0x00C8, // 87 E`    seen: IT (the capital alone, as Italian writes it)
    0x00C9, // 88 E'    seen: ES
    0x00CA, // 89 E^    order
    0x00CB, // 8A E"    order
    0x00CC, // 8B I`    order
    0x00CD, // 8C I'    seen: ES
    0x00CE, // 8D I^    order
    0x00CF, // 8E I"    order
    0x00D1, // 8F N~    seen: ES
    0x00D2, // 90 O`    seen: IT
    0x00D3, // 91 O'    seen: ES
    0x00D4, // 92 O^    order
    0x00D5, // 93 O~    order
    0x00D6, // 94 O"    seen: DE
    0x00D8, // 95 O/    order (the symmetry with B0)
    0x00D9, // 96 U`    seen: IT
    0x00DA, // 97 U'    seen: ES
    0x00DC, // 98 U"    seen: DE
    0x00DF, // 99 ss    seen: DE
    0x00E0, // 9A a`    seen: FR IT
    0x00E1, // 9B a'    seen: ES (a name elsewhere)
    0x00E2, // 9C a^    seen: FR
    0x00E3, // 9D a~    order
    0x00E4, // 9E a"    seen: DE
    0x00E5, // 9F a°    order
    0x00E6, // A0 ae    order
    0x00E7, // A1 c,    seen: FR
    0x00E8, // A2 e`    seen: FR IT
    0x00E9, // A3 e'    seen: FR IT ES (a name in EN DE)
    0x00EA, // A4 e^    seen: FR
    0x00EB, // A5 e"    seen: FR
    0x00EC, // A6 i`    seen: IT
    0x00ED, // A7 i'    seen: ES
    0x00EE, // A8 i^    seen: FR
    0x00EF, // A9 i"    seen: FR
    0x00F1, // AA n~    seen: ES
    0x00F2, // AB o`    seen: IT
    0x00F3, // AC o'    seen: ES (a name elsewhere)
    0x00F4, // AD o^    seen: FR
    0x00F5, // AE o~    order
    0x00F6, // AF o"    seen: DE
    0x00F8, // B0 o/    order
    0x00F9, // B1 u`    seen: FR IT
    0x00FA, // B2 u'    seen: ES (once in FR after a unit, where a superscript 2 was meant: the game draws this glyph there too)
    0x00FB, // B3 u^    seen: FR
    0x00FC, // B4 u"    seen: DE
    0x00B0, // B5 °     seen: FR IT ES
    0,      // B6       never seen
    0x2026, // B7 ...   seen: FR DE ES
    0x00AE, // B8 (R)   seen: EN FR DE IT ES
    0x00A0, // B9 nbsp  seen: EN FR DE IT ES (French's space before ? ! : %)
};
constexpr u8 kCharsetFirst = 0x7B;

void AppendUtf8(std::string& s, u32 cp)
{
    if (cp < 0x80) s.push_back((char)cp);
    else if (cp < 0x800) { s.push_back((char)(0xC0 | (cp >> 6))); s.push_back((char)(0x80 | (cp & 0x3F))); }
    else { s.push_back((char)(0xE0 | (cp >> 12))); s.push_back((char)(0x80 | ((cp >> 6) & 0x3F))); s.push_back((char)(0x80 | (cp & 0x3F))); }
}

void AppendHex(std::string& s, u8 b)
{
    static const char* const hex = "0123456789ABCDEF";
    s.push_back(hex[b >> 4]);
    s.push_back(hex[b & 15]);
}

// The game's bytes to UTF-8: nothing is dropped, what has no meaning yet stays visible.
std::string ToUtf8(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); i++)
    {
        const u8 b = (u8)raw[i];
        if (b == '\n' || (b >= 0x20 && b < kCharsetFirst)) out.push_back((char)b);
        else if (b >= 0xF0)
        {
            out.push_back('{');
            AppendHex(out, b);
            if (i + 1 < raw.size()) AppendHex(out, (u8)raw[++i]);
            out.push_back('}');
        }
        else if (b >= kCharsetFirst && b < kCharsetFirst + std::size(kCharset) && kCharset[b - kCharsetFirst]) AppendUtf8(out, kCharset[b - kCharsetFirst]);
        else { out.push_back('{'); AppendHex(out, b); out.push_back('}'); }
    }
    return out;
}

// The game's language -> file table (at 0x0211E7D4 in its code): Japanese, which the European
// game has no text for, reads the English file.
constexpr int kStringFiles[6] = { 123, 123, 3496, 3593, 4184, 7599 };
// What a string table may hold: the game's are 3127 strings of a few hundred bytes at most
// (about 100 KB a table); a file beyond these is not one of them.
constexpr size_t kMaxString = 4096;        // a string's bits must end before this many bytes
constexpr size_t kMaxStrings = 4u << 20;   // all of a table's bytes together
}

bool RomData::Open(const std::vector<u8>& rom)
{
    archive.clear();
    offsets.clear();
    strings.clear();
    stringsFile = -1;
    language = -1;
    u32 start = 0, end = 0;
    if (!RootFile(rom, "rom.bin", start, end)) return false;
    // The archive: a u32 offset table (its first entry is its own size, so the offset of
    // resource 0), one entry per resource in the order of the ids the game asks for; resource
    // i runs to entry i + 1, the last to the archive's end.
    const u32 size = end - start;
    if (size < 4) return false;
    const u32 tableBytes = Rd32(&rom[start]);
    if (tableBytes < 4 || tableBytes % 4 || tableBytes > size) return false;
    const u32 n = tableBytes / 4;
    offsets.reserve(n + 1);
    for (u32 i = 0; i < n; i++) offsets.push_back(Rd32(&rom[start + 4 * i]));
    offsets.push_back(size);
    for (u32 i = 0; i < n; i++)
        if (offsets[i] > offsets[i + 1]) { offsets.clear(); return false; }
    archive.assign(rom.begin() + start, rom.begin() + end);
    return true;
}

std::vector<u8> RomData::Resource(int id) const
{
    if (id < 0 || id >= ResourceCount()) return {};
    return std::vector<u8>(archive.begin() + offsets[id], archive.begin() + offsets[id + 1]);
}

bool RomData::LoadStrings(int language)
{
    strings.clear();
    stringsFile = -1;
    this->language = -1;
    if (language < 0 || language >= (int)std::size(kStringFiles) || !IsOpen()) return false;
    const std::vector<u8> d = Resource(kStringFiles[language]);
    // The table (no compression header: the file begins with the tree's end): u32 N; the
    // Huffman tree from 4 to N, u16 pairs (node 0x100 + i is pair i: bit 0's child, then bit
    // 1's; a child under 0x100 is a byte, 0 ending the string); then u32 offsets into the
    // file, one per string (the first offset is where the offsets end, so their number); each
    // string a stream of bits read least significant bit first from its offset.
    if (d.size() < 8) return false;
    const u32 n = Rd32(&d[0]);
    if (n < 8 || n % 4 || n > d.size() - 4) return false;
    const u32 nodes = (n - 4) / 4;
    const u32 first = Rd32(&d[n]);
    if (first < n || first > d.size() || (first - n) % 4) return false;
    const u32 count = (first - n) / 4;
    if (count > d.size() / 4) return false;
    strings.reserve(count);
    size_t total = 0;
    for (u32 i = 0; i < count; i++)
    {
        std::string raw;
        size_t pos = Rd32(&d[n + 4 * i]);
        int bit = 0;
        bool ended = false;
        while (!ended && raw.size() < kMaxString)
        {
            u32 node = 0x100;
            while (node >= 0x100)
            {
                const u32 pair = node - 0x100;
                if (pos >= d.size() || pair >= nodes) { strings.clear(); return false; }
                const int b = (d[pos] >> bit) & 1;
                if (++bit == 8) { bit = 0; pos++; }
                node = Rd16(&d[4 + 4 * pair + 2 * b]);
            }
            if (node == 0) ended = true;
            else raw.push_back((char)node);
        }
        total += raw.size();
        if (!ended || total > kMaxStrings) { strings.clear(); return false; }
        strings.push_back(ToUtf8(raw));
    }
    stringsFile = kStringFiles[language];
    this->language = language == 0 ? 1 : language;
    return true;
}

std::string DropEscapes(const std::string& in)
{
    std::string s;
    for (size_t i = 0; i < in.size(); i++)
    {
        if (in[i] == '{')
        {
            const size_t e = in.find('}', i);
            if (e != std::string::npos && e - i <= 8) { i = e; continue; }
        }
        s += in[i];
    }
    return s;
}

std::string GameTextToUtf8(const std::string& raw) { return ToUtf8(raw); }
bool GameTextByte(unsigned char b) { return (b >= 0x20 && b < kCharsetFirst) || (b >= kCharsetFirst && b < kCharsetFirst + std::size(kCharset) && kCharset[b - kCharsetFirst]); }

std::string GameLower(const std::string& utf8)
{
    auto inCharset = [](u32 cp) {
        for (u16 c : kCharset)
            if (c == cp) return true;
        return false;
    };
    std::string out;
    for (size_t i = 0; i < utf8.size();)
    {
        const u8 b = (u8)utf8[i];
        // One code point (1 to 3 bytes: the game's charset has none past the BMP).
        u32 cp = b;
        size_t n = 1;
        if (b >= 0xE0 && i + 2 < utf8.size()) { cp = ((b & 0x0F) << 12) | (((u8)utf8[i + 1] & 0x3F) << 6) | ((u8)utf8[i + 2] & 0x3F); n = 3; }
        else if (b >= 0xC0 && i + 1 < utf8.size()) { cp = ((b & 0x1F) << 6) | ((u8)utf8[i + 1] & 0x3F); n = 2; }
        // The pairs: A-Z / a-z, and a Latin-1 capital whose small letter (+ 0x20) the game has too.
        if (cp >= 'A' && cp <= 'Z') cp += 0x20;
        else if (cp >= 0xC0 && cp <= 0xDE && cp != 0xD7 && inCharset(cp) && inCharset(cp + 0x20)) cp += 0x20;
        if (n == 1) out.push_back((char)cp);
        else AppendUtf8(out, cp);
        i += n;
    }
    return out;
}

std::string RomData::String(int id) const
{
    if (id < 0 || id >= StringCount()) return "";
    return strings[id];
}

// --- Compression ---------------------------------------------------------------------------
namespace
{
// The bits as the game's own decoder (kind 6) reads them: 32-bit words, little-endian, each
// word's bits from its most significant one. `bad` once the stream has run out (every read
// then gives 0, and the caller refuses the result). A last word short of 4 bytes is padded
// with zeros: the stream's bits end inside it.
struct BitReader
{
    const u8* p;
    size_t len;
    size_t pos = 0;        // the next word's first byte
    u32 buf = 0x80000000;  // the bits left of the word being read, then a 1 (the decoder's sentinel)
    bool bad = false;
    u32 Bit()
    {
        u32 bit = buf >> 31;
        buf <<= 1;
        if (buf == 0)
        {
            if (pos >= len) { bad = true; return 0; }
            u32 w = 0;
            for (int i = 0; i < 4; i++) if (pos + i < len) w |= (u32)p[pos + i] << (8 * i);
            pos += 4;
            bit = w >> 31;
            buf = (w << 1) | 1;
        }
        return bit;
    }
    u32 Bits(u32 n) { u32 v = 0; for (u32 i = 0; i < n; i++) v = (v << 1) | Bit(); return v; }
    // The decoder's number code: n ones (7 at most) and, under 7 of them, a zero, then n
    // bits: (1 << n) | bits, 1 to 255.
    u32 Number() { u32 n = 0; while (n < 7 && Bit()) n++; return (1u << n) | Bits(n); }
};

// Kind 6, the game's own, ported from the routine the game keeps at 0x01FF8000 (ITCM; read
// in its disassembly, checked against its output under an emulator on every compressed
// resource of the European game). The stream's first word: byte 0 the size N of a table of
// run bytes (4 to 32, a multiple of 4), byte 1 the prefix K, byte 2 the extra offset bits
// B2 (0 in the game's data), byte 3 the prefix's width B3 (1 to 4 in the game's data); then
// the table; then codes. A code starts with B3 bits: not K, they are the top of a literal
// byte whose other 8 - B3 bits follow. K, then a number G: G >= 2 is a copy of G + 1 bytes
// from a distance of 1 + the offset, a number G2 (255: the end of the stream) less 1, B2
// bits below it, a byte below those; G == 1 then two bits: 0 → a copy of 2 bytes from a
// distance of 1 + a byte; 1, 0 → a literal whose top B3 bits are the old K, the new K read
// (B3 bits) before its low 8 - B3 bits; 1, 1 → a run: a number C (C >= 128: one more bit
// makes the count's low byte (2C + bit) & 0xFF and a number less 1 its high byte, else C
// the low byte), the count being the low byte + 1 + (high << 8), of the byte at number I
// (I < 32: table[I - 1], else (I << 3 | 3 bits) & 0xFF). The game writes halfwords and keeps
// an odd byte pending; the bytes come out the same, so this writes bytes.
bool DecodeGame(const u8* d, size_t len, size_t size, std::vector<u8>& out)
{
    if (len < 4) return false;
    const u32 h = Rd32(d);
    const u32 n = h & 0xFF, b2 = (h >> 16) & 0xFF, b3 = h >> 24;
    u32 k = (h >> 8) & 0xFF;
    if (n == 0 || n > 32 || n % 4 || b3 > 8 || b2 > 16 || k >= (1u << b3)) return false;
    if (len < 4 + n) return false;
    const u8* table = d + 4;
    const u32 low = 8 - b3;
    BitReader r {d + 4 + n, len - 4 - n};
    out.clear();
    out.reserve(size);
    auto put = [&](u8 b) { if (out.size() >= size) return false; out.push_back(b); return true; };
    auto copy = [&](size_t dist, size_t count) {
        if (dist == 0 || dist > out.size() || count > size - out.size()) return false;
        for (size_t i = 0; i < count; i++) { const u8 b = out[out.size() - dist]; out.push_back(b); }
        return true;
    };
    for (;;)
    {
        if (r.bad) return false;
        const u32 v = r.Bits(b3);
        if (v != k)
        {
            if (!put((u8)((v << low) | r.Bits(low)))) return false;
            continue;
        }
        const u32 g = r.Number();
        if (g >= 2)
        {
            const u32 g2 = r.Number();
            if (g2 == 255) break;
            u32 off = g2 - 1;
            if (b2) off = (off << b2) | r.Bits(b2);
            off = (off << 8) | r.Bits(8);
            if (r.bad || !copy((size_t)off + 1, (size_t)g + 1)) return false;
        }
        else if (!r.Bit())
        {
            const u32 off = r.Bits(8);
            if (r.bad || !copy((size_t)off + 1, 2)) return false;
        }
        else if (!r.Bit())
        {
            const u32 next = r.Bits(b3);
            const u8 b = (u8)((k << low) | r.Bits(low));
            k = next;
            if (r.bad || !put(b)) return false;
        }
        else
        {
            const u32 c = r.Number();
            u32 lo = c, hi = 0;
            if (c >= 128) { lo = (2 * c + r.Bit()) & 0xFF; hi = r.Number() - 1; }
            const size_t count = (size_t)lo + 1 + ((size_t)hi << 8);
            const u32 i = r.Number();
            u8 b;
            if (i < 32) { if (i - 1 >= n) return false; b = table[i - 1]; }
            else b = (u8)((i << 3) | r.Bits(3));
            if (r.bad || count > size - out.size()) return false;
            out.insert(out.end(), count, b);
        }
    }
    return !r.bad && out.size() == size;
}

// Kind 1, the DS BIOS's LZ10: a flag byte, then 8 units, from its top bit: set, two bytes
// (a length of 3 + the top nibble, a distance of 1 + the other 12 bits); clear, a literal.
// Stops at the size (a copy over it is cut there).
bool DecodeLz10(const u8* d, size_t len, size_t size, std::vector<u8>& out)
{
    out.clear();
    out.reserve(size);
    size_t pos = 0;
    while (out.size() < size)
    {
        if (pos >= len) return false;
        const u8 flags = d[pos++];
        for (int b = 0; b < 8 && out.size() < size; b++)
        {
            if (flags & (0x80 >> b))
            {
                if (pos + 2 > len) return false;
                const u32 x = ((u32)d[pos] << 8) | d[pos + 1];
                pos += 2;
                const size_t count = (x >> 12) + 3, dist = (x & 0xFFF) + 1;
                if (dist > out.size()) return false;
                for (size_t i = 0; i < count && out.size() < size; i++) { const u8 c = out[out.size() - dist]; out.push_back(c); }
            }
            else
            {
                if (pos >= len) return false;
                out.push_back(d[pos++]);
            }
        }
    }
    return true;
}
}

bool Decompress(const u8* data, size_t len, std::vector<u8>& out)
{
    out.clear();
    if (!data || len < 4) return false;
    const u32 h = Rd32(data);
    const u32 kind = (h >> 4) & 7;
    const size_t size = h >> 8;
    if ((h & 0x0F) || size > kMaxUnpacked) return false;
    bool ok = false;
    switch (kind)
    {
    case 0: ok = size <= len - 4; if (ok) out.assign(data + 4, data + 4 + size); break;
    case 1: ok = DecodeLz10(data + 4, len - 4, size, out); break;
    case 6: ok = DecodeGame(data + 4, len - 4, size, out); break;
    default: break;
    }
    if (!ok || out.size() != size) { out.clear(); return false; }
    // The 16-bit delta: each halfword is the sum of the one before it and itself.
    if (h & 0x80)
        for (size_t i = 2; i + 1 < out.size(); i += 2)
        {
            const u16 v = (u16)(Rd16(&out[i]) + Rd16(&out[i - 2]));
            out[i] = (u8)v;
            out[i + 1] = (u8)(v >> 8);
        }
    return true;
}

bool Decompress(const std::vector<u8>& data, size_t offset, std::vector<u8>& out)
{
    if (offset >= data.size()) { out.clear(); return false; }
    return Decompress(data.data() + offset, data.size() - offset, out);
}

// --- Palettes and sprites ------------------------------------------------------------------
u32 Bgr555ToArgb(u16 bgr)
{
    auto c = [](u32 v) { v &= 31; return (v << 3) | (v >> 3); };
    return 0xFF000000 | c(bgr) << 16 | c(bgr >> 5) << 8 | c(bgr >> 10);
}

bool ReadPalette(const std::vector<u8>& res, std::vector<u32>& argb)
{
    argb.clear();
    if (res.size() != 32 && res.size() != 512) return false;
    argb.reserve(res.size() / 2);
    for (size_t i = 0; i + 1 < res.size(); i += 2) argb.push_back(Bgr555ToArgb(Rd16(&res[i])));
    return true;
}

namespace
{
// The DS's OBJ sizes: [shape][size], shapes square, wide, tall.
constexpr int kObjW[3][4] = {{8, 16, 32, 64}, {16, 32, 32, 64}, {8, 8, 16, 32}};
constexpr int kObjH[3][4] = {{8, 16, 32, 64}, {8, 8, 16, 32}, {16, 32, 32, 64}};

struct FrameHeader
{
    int pieces, w, h, x, y;
    size_t gfx;      // the frame's compressed tiles: a byte offset in the tiles resource
    size_t piecesAt; // its pieces' first byte in the descriptor
};
bool FrameAt(const std::vector<u8>& desc, int frame, FrameHeader& f)
{
    const int n = SpriteFrames(desc);
    if (frame < 0 || frame >= n) return false;
    const size_t p = 12 + Rd16(&desc[12 + 2 * (size_t)frame]);
    if (p + 10 > desc.size()) return false;
    f.pieces = Rd16(&desc[p]);
    f.w = desc[p + 2];
    f.h = desc[p + 3];
    f.gfx = Rd16(&desc[p + 4]);
    f.x = (s16)Rd16(&desc[p + 6]);
    f.y = (s16)Rd16(&desc[p + 8]);
    f.piecesAt = p + 10;
    return f.pieces >= 1 && f.pieces <= 64 && f.w > 0 && f.h > 0 && 4 * (size_t)f.pieces <= desc.size() - f.piecesAt;
}
}

int SpriteFrames(const std::vector<u8>& desc)
{
    if (desc.size() < 12) return 0;
    const int w = desc[0], h = desc[1], n = Rd16(&desc[6]);
    if (w == 0 || h == 0 || Rd16(&desc[2]) != 0 || n < 1 || n > 255 || 2 * (size_t)n > desc.size() - 12) return 0;
    return n;
}

bool DecodeSpriteFrame(const std::vector<u8>& tiles, const std::vector<u8>& desc, const std::vector<u8>& pal, int frame, SpriteFrame& out)
{
    out = SpriteFrame {};
    FrameHeader f;
    if (!FrameAt(desc, frame, f)) return false;
    std::vector<u32> colours;
    if (!ReadPalette(pal, colours) || colours.size() != 16) return false;
    std::vector<u8> px;
    if (!Decompress(tiles, f.gfx, px)) return false;
    std::vector<u32> argb((size_t)f.w * f.h, 0);
    for (int i = 0; i < f.pieces; i++)
    {
        const u32 pos = Rd16(&desc[f.piecesAt + 4 * (size_t)i]), attr = Rd16(&desc[f.piecesAt + 4 * (size_t)i + 2]);
        int x = pos & 0x1FF, y = (pos >> 9) & 0x7F;
        if (x & 0x100) x -= 0x200;
        if (y & 0x40) y -= 0x80;
        const u32 shape = (attr >> 4) & 3, size = (attr >> 2) & 3;
        if (shape == 3) return false;
        const int pw = kObjW[shape][size], ph = kObjH[shape][size];
        // The piece's tiles follow each other from its first (32 bytes each, 1D mapping).
        const size_t base = (size_t)(attr >> 6) * 32, need = (size_t)(pw / 8) * (ph / 8) * 32;
        if (base > px.size() || need > px.size() - base) return false;
        size_t o = base;
        for (int ty = 0; ty < ph / 8; ty++)
            for (int tx = 0; tx < pw / 8; tx++, o += 32)
                for (int yy = 0; yy < 8; yy++)
                {
                    const int Y = y - f.y + ty * 8 + yy;
                    if (Y < 0 || Y >= f.h) continue;
                    for (int xx = 0; xx < 4; xx++)
                    {
                        const u8 b = px[o + yy * 4 + xx];
                        for (int half = 0; half < 2; half++)
                        {
                            const u32 c = half ? b >> 4 : b & 15;
                            const int X = x - f.x + tx * 8 + xx * 2 + half;
                            if (c && X >= 0 && X < f.w) argb[(size_t)Y * f.w + X] = colours[c];
                        }
                    }
                }
    }
    out.w = f.w;
    out.h = f.h;
    out.pieces = f.pieces;
    out.argb = std::move(argb);
    return true;
}

int RomData::SpriteFrames(int desc) const { return sims2wide::SpriteFrames(Resource(desc)); }

bool RomData::Sprite(int tiles, int desc, int pal, int frame, SpriteFrame& out) const
{
    out = SpriteFrame {};
    if (!IsOpen()) return false;
    return DecodeSpriteFrame(Resource(tiles), Resource(desc), Resource(pal), frame, out);
}
}
