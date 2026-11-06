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
    return true;
}

std::string GameTextToUtf8(const std::string& raw) { return ToUtf8(raw); }

std::string RomData::String(int id) const
{
    if (id < 0 || id >= StringCount()) return "";
    return strings[id];
}
}
