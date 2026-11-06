// Checks RomData (romdata.*) on a synthetic ROM written by hand: a header, a name table, a
// file table, an archive and a tiny string table of made-up text, a few hundred bytes, no
// game data and no melonDS NDS. The valid ROM decodes; every truncation, oversize,
// out-of-range or cyclic table is refused without a crash (run once under
// -fsanitize=address,undefined too). Then the status bar's words (HudStringsFrom) from such a
// table of made-up text.
//   ninja -C build romdata_test && ./build/romdata_test
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "port/statusbar.h"
#include "romdata.h"

using namespace sims2wide;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

using Bytes = std::vector<u8>;
static void W16(Bytes& b, size_t at, u32 v) { b[at] = (u8)v; b[at + 1] = (u8)(v >> 8); }
static void W32(Bytes& b, size_t at, u32 v) { W16(b, at, v); W16(b, at + 2, v >> 16); }
static void Put16(Bytes& b, u32 v) { b.push_back((u8)v); b.push_back((u8)(v >> 8)); }
static void Put32(Bytes& b, u32 v) { Put16(b, v); Put16(b, v >> 16); }
static void PutName(Bytes& b, u8 type, const char* name) { b.push_back(type); while (*name) b.push_back((u8)*name++); }

// A string table in the game's format: u32 N, a balanced tree over the symbols (the terminator
// 0 among them) as u16 pairs up to N, u32 offsets, then each string's bytes and a 0 as bits,
// least significant first, from a byte boundary.
struct StringTable
{
    std::vector<u16> pairs;              // pair i: the children of node 0x100 + i
    std::map<u8, std::vector<int>> codes;

    int Build(const std::vector<u8>& syms, size_t lo, size_t hi, std::vector<int>& path)
    {
        if (hi - lo == 1) { codes[syms[lo]] = path; return syms[lo]; }
        const size_t k = pairs.size() / 2;
        pairs.push_back(0);
        pairs.push_back(0);
        const size_t mid = (lo + hi) / 2;
        path.push_back(0);
        const int left = Build(syms, lo, mid, path);
        path.back() = 1;
        const int right = Build(syms, mid, hi, path);
        path.pop_back();
        pairs[2 * k] = (u16)left;
        pairs[2 * k + 1] = (u16)right;
        return 0x100 + (int)k;
    }

    Bytes Make(const std::vector<Bytes>& strings)
    {
        std::vector<u8> syms = { 0, 'a' }; // two at least: the root must be a pair
        for (const Bytes& s : strings)
            for (u8 c : s) if (std::find(syms.begin(), syms.end(), c) == syms.end()) syms.push_back(c);
        pairs.clear();
        codes.clear();
        std::vector<int> path;
        Build(syms, 0, syms.size(), path);
        Bytes streams;
        std::vector<u32> starts;
        for (const Bytes& s : strings)
        {
            starts.push_back((u32)streams.size());
            size_t nbits = 0;
            auto put = [&](const std::vector<int>& code) {
                for (int bit : code)
                {
                    if (nbits % 8 == 0) streams.push_back(0);
                    streams.back() |= (u8)(bit << (nbits % 8));
                    nbits++;
                }
            };
            for (u8 c : s) put(codes[c]);
            put(codes[0]);
        }
        const u32 n = 4 + 2 * (u32)pairs.size();
        Bytes file;
        Put32(file, n);
        for (u16 p : pairs) Put16(file, p);
        for (u32 start : starts) Put32(file, n + 4 * (u32)starts.size() + start);
        file.insert(file.end(), streams.begin(), streams.end());
        return file;
    }
};

// A ROM holding the archive with the string file as resource `id` among `entries` (the others
// empty): the header, the name table (a directory, a file "a.bin", then "rom.bin"), the file
// table (two files), the archive.
struct Rom
{
    Bytes b;
    size_t fnt = 0, fat = 0, archive = 0;
    u32 fntSize = 0, fatSize = 16;
};
static Rom MakeRom(const Bytes& stringFile, u32 entries = 124, u32 id = 123)
{
    Rom r;
    r.b.assign(0x50, 0);
    Bytes fnt;
    Put32(fnt, 8); // the root's list follows its entry
    Put16(fnt, 0); // its first file id
    Put16(fnt, 1); // one directory in all
    PutName(fnt, 0x80 | 6, "Movies");
    Put16(fnt, 0xF001);
    PutName(fnt, 5, "a.bin");
    PutName(fnt, 7, "rom.bin");
    fnt.push_back(0);
    r.fnt = r.b.size();
    r.fntSize = (u32)fnt.size();
    r.b.insert(r.b.end(), fnt.begin(), fnt.end());
    while (r.b.size() % 4) r.b.push_back(0);
    Bytes arch;
    for (u32 i = 0; i < entries; i++) Put32(arch, 4 * entries + (i > id ? (u32)stringFile.size() : 0));
    arch.insert(arch.end(), stringFile.begin(), stringFile.end());
    r.fat = r.b.size();
    const u32 start = (u32)(r.fat + r.fatSize);
    Put32(r.b, start); Put32(r.b, start);                        // a.bin, empty
    Put32(r.b, start); Put32(r.b, start + (u32)arch.size());     // rom.bin
    r.archive = r.b.size();
    r.b.insert(r.b.end(), arch.begin(), arch.end());
    W32(r.b, 0x40, (u32)r.fnt); W32(r.b, 0x44, r.fntSize); W32(r.b, 0x48, (u32)r.fat); W32(r.b, 0x4C, r.fatSize);
    return r;
}

static bool Opens(const Bytes& rom) { RomData d; return d.Open(rom); }
static bool Loads(const Bytes& stringFile) { RomData d; return d.Open(MakeRom(stringFile).b) && d.LoadStrings(1); }

int main()
{
    // The valid ROM: the archive found, its resources, the strings decoded and mapped.
    StringTable table;
    const Bytes s0 = { 'H', 'i', '!', '\n', '{' };
    const Bytes s1 = { 0xA3, 0xB9, 0xB6, 0x12, 0xF0, 0x41, 0xF0 };
    const Bytes s2 = {};
    const Bytes s3 = { 0x7F, 0x99, 0xB4, 0xB7, 'a', 'b', 'c', 'd', 'e' };
    const Bytes file = table.Make({ s0, s1, s2, s3 });
    const Rom rom = MakeRom(file);
    CHECK(rom.b.size() < 1000);
    {
        RomData d;
        CHECK(!d.IsOpen() && d.ResourceCount() == 0 && d.Resource(0).empty() && !d.LoadStrings(1) && d.StringCount() == 0 && d.String(0) == "");
        CHECK(d.Open(rom.b));
        CHECK(d.IsOpen() && d.ResourceCount() == 124);
        CHECK(d.Resource(123) == file);
        CHECK(d.Resource(0).empty() && d.Resource(122).empty());
        CHECK(d.Resource(-1).empty() && d.Resource(124).empty());
        CHECK(d.LoadStrings(1) && d.StringsFile() == 123);
        CHECK(d.StringCount() == 4);
        CHECK(d.String(0) == "Hi!\n\xC2\xA9");                                       // '{' is the copyright sign
        CHECK(d.String(1) == "\xC3\xA9\xC2\xA0{B6}{12}{F041}{F0}");                   // e acute, a no-break space, an unknown byte, a control byte, an escape, a cut escape
        CHECK(d.String(2) == "");
        CHECK(d.String(3) == "\xC3\x80\xC3\x9F\xC3\xBC\xE2\x80\xA6" "abcde");        // A grave, sharp s, u umlaut, an ellipsis
        CHECK(d.String(4) == "" && d.String(-1) == "");
        CHECK(d.LoadStrings(0) && d.StringsFile() == 123 && d.StringCount() == 4); // Japanese reads the English file
        CHECK(!d.LoadStrings(2) && d.StringCount() == 0 && d.StringsFile() == -1); // the French file is not in this table
        CHECK(!d.LoadStrings(6) && !d.LoadStrings(-1));
        CHECK(d.Open(rom.b) && d.StringCount() == 0);                                // opening again forgets the strings
        CHECK(d.LoadStrings(1) && d.StringCount() == 4);
        d.DropArchive();                                                             // the strings stay, the archive goes
        CHECK(!d.IsOpen() && d.Resource(123).empty() && d.StringCount() == 4 && d.String(0) == "Hi!\n\xC2\xA9");
    }
    // The bit order: a tree by hand (pair 0: bit 0 'a', bit 1 pair 1; pair 1: bit 0 the end,
    // bit 1 'b'), one string whose bits 0, 1 1, 1 0 read "ab".
    {
        Bytes f;
        Put32(f, 12);
        Put16(f, 'a'); Put16(f, 0x101);
        Put16(f, 0); Put16(f, 'b');
        Put32(f, 16);
        f.push_back(0x0E);
        RomData d;
        CHECK(d.Open(MakeRom(f).b) && d.LoadStrings(1) && d.StringCount() == 1 && d.String(0) == "ab");
        // The same tree with 600 bytes of bit 0: a string of 4800 'a's never ending: refused.
        Bytes g(f.begin(), f.begin() + 16);
        g.insert(g.end(), 600, 0);
        CHECK(!Loads(g));
    }

    // Refused ROMs: each a copy of the valid one, broken in one way.
    CHECK(!Opens(Bytes(0x4F, 0)));
    CHECK(!Opens(Bytes(0x200, 0)));
    auto broken = [&](size_t at, u32 v) { Bytes b = rom.b; W32(b, at, v); return b; };
    CHECK(!Opens(broken(0x44, 0xFFFFFFF0)));                  // FNT size over the file (no underflow)
    CHECK(!Opens(broken(0x44, (u32)rom.b.size())));           // FNT size the file's: past the end from its offset
    CHECK(!Opens(broken(0x44, 7)));                           // FNT too small for the root
    CHECK(!Opens(broken(0x40, (u32)rom.b.size())));           // FNT offset at the end
    CHECK(!Opens(broken(0x40, 0xFFFFFFF0)));                  // FNT offset far beyond
    CHECK(!Opens(broken(0x4C, 0xFFFFFFF0)));                  // FAT size over the file
    CHECK(!Opens(broken(0x48, (u32)rom.b.size() - 8)));       // FAT offset leaving no room for file 1
    CHECK(!Opens(broken(0x48, 0xFFFFFFF0)));                  // FAT offset far beyond
    CHECK(!Opens(broken(0x4C, 8)));                           // FAT holding file 0 only
    CHECK(!Opens(broken(0x4C, 0)));                           // no FAT
    CHECK(!Opens(broken(rom.fnt, 0)));                        // the root's list inside the root's entry
    CHECK(!Opens(broken(rom.fnt, rom.fntSize)));              // the root's list at the FNT's end
    CHECK(!Opens(broken(rom.fnt, 0xFFFFFF00)));               // the root's list far beyond
    CHECK(!Opens(broken(0x44, 8 + 3)));                       // FNT cut inside the directory's name
    CHECK(!Opens(broken(0x44, 8 + 9 + 3)));                   // FNT cut inside "a.bin"
    CHECK(!Opens(broken(0x44, 8 + 9 + 6 + 4)));               // FNT cut inside "rom.bin"
    CHECK(!Opens(broken(rom.fat + 8, (u32)rom.b.size())));    // rom.bin starting after its end
    CHECK(!Opens(broken(rom.fat + 12, (u32)rom.b.size() + 1))); // rom.bin ending past the file
    CHECK(!Opens(broken(rom.fat + 12, 0xFFFFFFFF)));
    CHECK(!Opens(broken(rom.fat + 12, (u32)rom.archive + 2))); // rom.bin of 2 bytes
    CHECK(!Opens(broken(rom.archive, 0)));                    // the archive's table of no entry
    CHECK(!Opens(broken(rom.archive, 6)));                    // not a multiple of 4
    CHECK(!Opens(broken(rom.archive, 0x7FFFFFF0)));           // larger than the archive
    CHECK(!Opens(broken(rom.archive, (u32)(rom.b.size() - rom.archive) + 4)));
    CHECK(!Opens(broken(rom.archive + 4 * 5, 4 * 124 + 1000))); // entry 5 after entry 6
    CHECK(!Opens(broken(rom.archive + 4 * 123, (u32)(rom.b.size() - rom.archive) + 1))); // the last entry past the end
    CHECK(Opens(broken(rom.archive + 4 * 123, (u32)(rom.b.size() - rom.archive)))); // (at the end: an empty last resource is fine)
    {
        Bytes b = rom.b;
        b[rom.fnt + 8 + 9 + 5 + 1] = 'x'; // "rom.bin" misspelt: not found
        CHECK(!Opens(b));
        Bytes c = rom.b;
        c[rom.fnt + 8 + 9] = 0; // the list ends before "a.bin"
        CHECK(!Opens(c));
    }

    // Refused string files, the ROM around them valid.
    CHECK(Loads(file));
    CHECK(!Loads(Bytes()));
    CHECK(!Loads(Bytes(4, 0)));
    CHECK(!Loads(Bytes(7, 0)));
    auto cut = [&](size_t at, u32 v) { Bytes f = file; W32(f, at, v); return f; };
    const u32 n = (u32)file[0] | ((u32)file[1] << 8);
    CHECK(!Loads(cut(0, 0)));                                  // no tree
    CHECK(!Loads(cut(0, 6)));                                  // N not a multiple of 4
    CHECK(!Loads(cut(0, 4)));                                  // no pair at all
    CHECK(!Loads(cut(0, (u32)file.size())));                   // N at the end: no offsets
    CHECK(!Loads(cut(0, (u32)file.size() + 4)));
    CHECK(!Loads(cut(0, 0xFFFFFFFC)));
    CHECK(!Loads(cut(n, n - 4)));                              // the first offset before the offsets
    CHECK(!Loads(cut(n, (u32)file.size() + 1)));               // past the end
    CHECK(!Loads(cut(n, 0xFFFFFFFF)));
    CHECK(!Loads(cut(n, n + 6)));                              // offsets not a multiple of 4
    CHECK(!Loads(cut(n + 4, (u32)file.size())));               // a string starting at the end
    CHECK(!Loads(cut(n + 4, 0xFFFFFFFF)));
    {
        Bytes f = file;
        W16(f, 4 + 2, 0x100 + 999);                            // the root's bit-1 child: no such pair
        CHECK(!Loads(f));
        Bytes g = file;
        W16(g, 4, 0x100); W16(g, 6, 0x100);                    // a cycle: every bit goes back to the root
        CHECK(!Loads(g));
        Bytes h = file;
        W16(h, 4, 0x100 + (n - 4) / 4);                        // the first pair past the tree
        CHECK(!Loads(h));
        Bytes k(file.begin(), file.end() - 1);                 // the last string's bits cut
        CHECK(!Loads(k));
        Bytes m(file.begin(), file.begin() + n + 16);          // the streams cut away
        CHECK(!Loads(m));
    }

    // The status bar's words (port/statusbar.h) from a table of made-up text at the game's ids:
    // 30 the clock's format, 178 / 179 am / pm, 28 the money's, 2421 the meter's label.
    {
        auto hudTable = [](const Bytes& clock, const Bytes& money, const Bytes& label, size_t count = 2422) {
            std::vector<Bytes> v(count);
            auto set = [&](size_t id, const Bytes& b) { if (id < v.size()) v[id] = b; };
            set(30, clock);
            set(178, {'a', 'm'});
            set(179, {'p', 'm'});
            set(28, money);
            set(2421, label);
            StringTable t;
            return t.Make(v);
        };
        const Bytes clock = {'@', 'h', 'h', ':', '@', 'm', 'm', ' ', '@', 'a', 'm'};
        const Bytes money = {'@', '1', 0xB9, '$'};         // a no-break space (0xB9) before the sign
        const Bytes label = {' ', 'M', 'e', 't', 'e', 'r', 0xB9, ':', ' '};
        auto words = [](const Bytes& file) {
            RomData d;
            const bool ok = d.Open(MakeRom(file).b) && d.LoadStrings(1);
            return ok ? HudStringsFrom(&d) : HudStrings {};
        };
        const HudStrings h = words(hudTable(clock, money, label));
        CHECK(h.ok && h.clock == "@hh:@mm @am" && h.am == "am" && h.pm == "pm" && h.money == "@1\xC2\xA0$");
        CHECK(h.label == "Meter"); // the colon, the no-break space and the outer spaces trimmed
        // A label with a placeholder in it is dropped (no label), the words still ok.
        const HudStrings at = words(hudTable(clock, money, {'M', '@', '1', ':'}));
        CHECK(at.ok && at.label.empty());
        // Formats without their placeholders: not ok (no bar, the game's own HUD).
        CHECK(!words(hudTable({'@', 'h', 'h', ':', '0', '0'}, money, label)).ok);
        CHECK(!words(hudTable({'@', 'm', 'm'}, money, label)).ok);
        CHECK(!words(hudTable(clock, {'$', '1'}, label)).ok);
        // A table too short for the label's id, or none at all: not ok.
        CHECK(!words(hudTable(clock, money, label, 2421)).ok);
        CHECK(!HudStringsFrom(nullptr).ok);
        RomData closed;
        CHECK(!HudStringsFrom(&closed).ok);
    }

    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("romdata_test: ok\n");
    return 0;
}
