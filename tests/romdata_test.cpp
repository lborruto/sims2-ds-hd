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

#include "port/icons.h"
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
static Rom MakeRomFrom(const std::vector<Bytes>& resources)
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
    const u32 entries = (u32)resources.size();
    u32 at = 4 * entries;
    for (const Bytes& res : resources) { Put32(arch, at); at += (u32)res.size(); }
    for (const Bytes& res : resources) arch.insert(arch.end(), res.begin(), res.end());
    r.fat = r.b.size();
    const u32 start = (u32)(r.fat + r.fatSize);
    Put32(r.b, start); Put32(r.b, start);                        // a.bin, empty
    Put32(r.b, start); Put32(r.b, start + (u32)arch.size());     // rom.bin
    r.archive = r.b.size();
    r.b.insert(r.b.end(), arch.begin(), arch.end());
    W32(r.b, 0x40, (u32)r.fnt); W32(r.b, 0x44, r.fntSize); W32(r.b, 0x48, (u32)r.fat); W32(r.b, 0x4C, r.fatSize);
    return r;
}
static Rom MakeRom(const Bytes& stringFile, u32 entries = 124, u32 id = 123)
{
    std::vector<Bytes> resources(entries);
    resources[id] = stringFile;
    return MakeRomFrom(resources);
}

static bool Opens(const Bytes& rom) { RomData d; return d.Open(rom); }
static bool Loads(const Bytes& stringFile) { RomData d; return d.Open(MakeRom(stringFile).b) && d.LoadStrings(1); }

// A stream in the game's own compression (kind 6), written as its decoder reads it: 32-bit
// little-endian words, each word's bits from the top; the codes as romdata.cpp describes
// them. `plain` is what the stream must decode to.
struct Game6
{
    u32 b3, k, b2, k0;
    Bytes table;
    Bytes words, plain;
    u32 cur = 0;
    int nbits = 0;

    Game6(u32 b3, u32 k, u32 b2, Bytes table) : b3(b3), k(k), b2(b2), k0(k), table(std::move(table)) {}
    void Bit(u32 b)
    {
        cur = (cur << 1) | (b & 1);
        if (++nbits == 32) Flush();
    }
    void Flush()
    {
        if (!nbits) return;
        Put32(words, cur << (32 - nbits));
        cur = 0;
        nbits = 0;
    }
    void Bits(u32 v, u32 n) { for (int i = (int)n - 1; i >= 0; i--) Bit(v >> i); }
    // The number code: n ones (a zero after them under 7), then n bits.
    void Number(u32 v)
    {
        u32 n = 0;
        while ((2u << n) <= v) n++;
        for (u32 i = 0; i < n; i++) Bit(1);
        if (n < 7) Bit(0);
        Bits(v & ((1u << n) - 1), n);
    }
    void Prefix() { Bits(k, b3); }
    u32 Top(u8 b) const { return b >> (8 - b3); }
    // A literal whose top bits are not the prefix.
    void Literal(u8 b) { Bits(b, 8); plain.push_back(b); }
    // A literal whose top bits are the prefix, and the prefix after it.
    void Escape(u8 b, u32 newK)
    {
        Prefix(); Number(1); Bit(1); Bit(0);
        Bits(newK, b3);
        Bits(b & ((1u << (8 - b3)) - 1), 8 - b3);
        k = newK;
        plain.push_back(b);
    }
    void Put(u8 b) { if (Top(b) == k) Escape(b, k); else Literal(b); }
    // (A copy from before the start, written to be refused, copies zeros here.)
    void Copied(u32 dist, u32 len) { for (u32 i = 0; i < len; i++) plain.push_back(dist <= plain.size() ? plain[plain.size() - dist] : 0); }
    void Short(u32 dist) { Prefix(); Number(1); Bit(0); Bits(dist - 1, 8); Copied(dist, 2); }
    void Copy(u32 dist, u32 len) // len 3 to 256
    {
        Prefix(); Number(len - 1);
        const u32 off = dist - 1, high = off >> 8;
        Number((high >> b2) + 1);
        if (b2) Bits(high & ((1u << b2) - 1), b2);
        Bits(off & 0xFF, 8);
        Copied(dist, len);
    }
    void Run(u8 b, u32 count, int tableIndex) // count 2 to 65280; tableIndex -1: the byte spelt out
    {
        Prefix(); Number(1); Bit(1); Bit(1);
        const u32 c = count - 1;
        if (c < 128) Number(c);
        else { Number(128 + ((c & 0xFF) >> 1)); Bit(c & 1); Number((c >> 8) + 1); }
        if (tableIndex >= 0) Number((u32)tableIndex + 1);
        else { Number(32 + (b >> 3)); Bits(b & 7, 3); }
        plain.insert(plain.end(), count, b);
    }
    void End() { Prefix(); Number(2); Number(255); }
    // The resource: the 4-byte header (size: the plain bytes', or `size`), the stream's word,
    // the table, the words.
    Bytes Stream(bool delta = false, long size = -1)
    {
        Flush();
        Bytes s;
        Put32(s, (u32)(size < 0 ? plain.size() : (size_t)size) << 8 | 0x60 | (delta ? 0x80 : 0));
        Put32(s, (u32)table.size() | k0 << 8 | b2 << 16 | b3 << 24);
        s.insert(s.end(), table.begin(), table.end());
        s.insert(s.end(), words.begin(), words.end());
        return s;
    }
};

static bool Unpacks(const Bytes& s, Bytes& out) { return Decompress(s.data(), s.size(), out); }
static bool Refused(const Bytes& s) { Bytes out; const bool ok = Decompress(s.data(), s.size(), out); return !ok && out.empty(); }

static void compression()
{
    // The game's own kind on every code: literals, escaped literals (the prefix kept, then
    // changed), runs from the table and spelt out, short and long (odd and even, overlapping
    // and not), copies near and far, with and without extra offset bits.
    {
        Game6 g(3, 4, 0, {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC});
        for (u8 b : Bytes {0x01, 0x7F, 0xFF, 0x00, 0x9F, 0x80}) g.Put(b); // 0x9F, 0x80 take the escape (top bits 100 = 4)
        g.Escape(0x85, 7);                                               // the prefix becomes 7 (0xE0 to 0xFF)
        g.Put(0x9F);                                                     // a plain literal now
        g.Put(0xE5);                                                     // escaped
        g.Run(0x33, 2, 2);
        g.Run(0x11, 127, 0);
        g.Run(0x5A, 128, -1);
        g.Run(0xCC, 129, 11);
        g.Run(0x07, 300, -1);
        g.Run(0xFE, 1000, -1);
        g.Short(1);
        g.Short(2);
        g.Short(256);
        g.Copy(1, 3);
        g.Copy(2, 5);
        g.Copy(3, 256);
        g.Copy(7, 4);
        g.Copy(1000, 6);
        g.Copy((u32)g.plain.size(), 9);
        g.Put(0x42);
        g.Short(1);
        g.End();
        const Bytes s = g.Stream();
        Bytes out;
        CHECK(Unpacks(s, out) && out == g.plain);
        CHECK(out.size() > 1800 && out[0] == 0x01 && out[4] == 0x9F && out[6] == 0x85 && out[7] == 0x9F && out[8] == 0xE5);
        // A delta stream of the same: the halfwords are summed after decoding.
        Game6 d(3, 4, 0, g.table);
        Bytes want = {0x10, 0x20, 0x35, 0x21, 0x40, 0x22, 0xFF, 0xFF, 0x05, 0x00, 0x07};
        u32 prev = 0;
        for (size_t i = 0; i + 1 < want.size(); i += 2)
        {
            const u32 v = want[i] | want[i + 1] << 8, diff = (v - prev) & 0xFFFF;
            prev = v;
            d.Put((u8)diff);
            d.Put((u8)(diff >> 8));
        }
        d.Put(want.back()); // an odd byte, left as it is
        d.End();
        CHECK(Unpacks(d.Stream(true), out) && out == want);
        // Every truncation of the stream is refused or gives the same bytes, never a crash;
        // cutting a whole word off the end, or more, is refused (the end code is gone).
        for (size_t n = 0; n < s.size(); n++)
        {
            Bytes cut(s.begin(), s.begin() + n);
            CHECK(!Unpacks(cut, out) || out == g.plain);
            if (n <= s.size() - 4) CHECK(Refused(cut));
        }
        // The header's size lying either way.
        CHECK(Refused(g.Stream(false, (long)g.plain.size() + 1)));
        CHECK(Refused(g.Stream(false, (long)g.plain.size() - 1)));
        CHECK(Refused(g.Stream(false, 0)));
        // The stream's word lying: no table, too big a table, one not a multiple of 4, a prefix
        // wider than a byte, a prefix out of its width, too many offset bits.
        auto word = [&](u32 v) { Bytes b = s; W32(b, 4, v); return b; };
        const u32 w = (u32)g.table.size() | 4 << 8 | 3 << 24;
        CHECK(!Refused(word(w)));
        CHECK(Refused(word(w & ~0xFFu)));
        CHECK(Refused(word((w & ~0xFFu) | 36)));
        CHECK(Refused(word((w & ~0xFFu) | 6)));
        CHECK(Refused(word((w & 0xFFFFFF) | 9u << 24)));
        CHECK(Refused(word((w & ~0xFF00u) | 8 << 8)));
        CHECK(Refused(word(w | 17 << 16)));
        // Another kind, the low nibble set, a size over the limit.
        auto header = [&](u32 v) { Bytes b = s; W32(b, 0, v); return b; };
        const u32 h = (u32)g.plain.size() << 8 | 0x60;
        for (u32 kind : {2u, 3u, 4u, 5u, 7u}) CHECK(Refused(header((h & ~0x70u) | kind << 4)));
        CHECK(Refused(header(h | 1)));
        CHECK(Refused(header((u32)(kMaxUnpacked + 1) << 8 | 0x60)));
        CHECK(Refused(Bytes {0x60, 0x10, 0x00}));
        CHECK(Refused(Bytes {}));
    }
    // The widths at their ends: no prefix bits (every literal escaped), eight (no low bits),
    // with extra offset bits, a run from a 4-byte table and a copy of the longest kind.
    {
        Game6 g(0, 0, 2, {0xAB, 0xCD, 0xEF, 0x01});
        for (u8 b : Bytes {0x00, 0xFF, 0x80, 0x7F}) g.Put(b);
        g.Run(0xEF, 65280, 2);
        g.Copy(65000, 256);
        g.Copy(1, 256);
        g.Run(0x01, 2, 3);
        g.End();
        Bytes out;
        CHECK(Unpacks(g.Stream(), out) && out == g.plain && out.size() == 4 + 65280 + 512 + 2);
        Game6 e(8, 0x42, 0, {1, 2, 3, 4});
        for (u8 b : Bytes {0x42, 0x41, 0x42, 0x00}) e.Put(b);
        e.Escape(0x42, 0x00);
        e.Put(0x00);
        e.Put(0x42);
        e.End();
        CHECK(Unpacks(e.Stream(), out) && out == e.plain && out.size() == 7);
    }
    // Copies and runs that cannot be: from before the start, past the size, a run byte past
    // the table, no end code at the size.
    {
        Game6 g(3, 4, 0, {1, 2, 3, 4});
        g.Short(1);
        g.End();
        CHECK(Refused(g.Stream()));
        Game6 c(3, 4, 0, {1, 2, 3, 4});
        c.Put(0x10);
        c.Copy(2, 3);
        c.End();
        CHECK(Refused(c.Stream()));
        Game6 t(3, 4, 0, {1, 2, 3, 4});
        t.Run(0x05, 3, 4); // index 5 of a table of 4
        t.End();
        CHECK(Refused(t.Stream()));
        Game6 n(3, 4, 0, {1, 2, 3, 4});
        n.Put(0x10);
        n.Put(0x11);
        CHECK(Refused(n.Stream(false, 2)));
        CHECK(Refused(n.Stream(false, 1)));
        // A stream of nothing: the end code alone gives 0 bytes.
        Game6 z(3, 4, 0, {1, 2, 3, 4});
        z.End();
        Bytes out = {1};
        CHECK(Unpacks(z.Stream(), out) && out.empty());
    }
    // Stored (kind 0): the bytes after the header, the size's worth, never more than there are.
    {
        Bytes s = {0x00, 0x05, 0x00, 0x00, 'a', 'b', 'c', 'd', 'e', 'f'};
        Bytes out;
        CHECK((Unpacks(s, out) && out == Bytes {'a', 'b', 'c', 'd', 'e'}));
        W32(s, 0, 7 << 8);
        CHECK(Refused(s));
        W32(s, 0, 6 << 8);
        CHECK(Unpacks(s, out) && out.size() == 6);
        W32(s, 0, 6 << 8 | 0x80); // the delta: (0x6261, 0x6463 + 0x6261, 0x6665 + that)
        CHECK((Unpacks(s, out) && out == Bytes {'a', 'b', 0xC4, 0xC6, 0x29, 0x2D}));
        W32(s, 0, 5 << 8 | 0x80); // an odd byte left alone
        CHECK((Unpacks(s, out) && out == Bytes {'a', 'b', 0xC4, 0xC6, 'e'}));
    }
    // LZ10 (kind 1): literals, a copy, an overlapping one, a copy cut at the size; cut
    // streams and a copy from before the start refused.
    {
        Bytes s;
        Put32(s, 12 << 8 | 0x10);
        s.push_back(0x08);                                  // 0 0 0 0 1 0 0 0: 4 literals, a copy, 3 literals
        s.insert(s.end(), {'a', 'b', 'c', 'd'});
        s.push_back(0x10); s.push_back(0x03);               // length 4, distance 4: abcd again
        s.insert(s.end(), {'x', 'y', 'z'});
        s.push_back(0x80);                                  // a copy: length 3, distance 1: zzz, cut at 12 (one z)
        s.push_back(0x00); s.push_back(0x00);
        Bytes out;
        CHECK((Unpacks(s, out) && out == Bytes {'a', 'b', 'c', 'd', 'a', 'b', 'c', 'd', 'x', 'y', 'z', 'z'}));
        for (size_t n = 4; n < s.size(); n++) CHECK(Refused(Bytes(s.begin(), s.begin() + n)));
        Bytes far = s;
        far[10] = 0x04; // distance 5 at 4 bytes written
        CHECK(Refused(far));
        Bytes back = {0x10, 0x03, 0x00, 0x00, 0x80, 0x00, 0x00};
        CHECK(Refused(back));
    }
}

// A sprite: its descriptor, its tiles resource (each frame's 4bpp tiles stored, kind 0, at
// its byte offset) and a 16-colour palette, as the game lays them out.
struct Sprite
{
    struct Piece { int x, y, shape, size, tile; };
    struct Frame { int w, h, x, y; std::vector<Piece> pieces; Bytes tiles; };
    int w = 16, h = 16;
    std::vector<Frame> frames;

    Bytes Descriptor() const
    {
        Bytes d;
        d.push_back((u8)w); d.push_back((u8)h);
        Put16(d, 0); Put16(d, 0); Put16(d, (u32)frames.size()); Put32(d, 0);
        std::vector<size_t> at;
        Bytes body;
        u32 gfx = 0;
        for (const Frame& f : frames)
        {
            at.push_back(body.size());
            Put16(body, (u32)f.pieces.size());
            body.push_back((u8)f.w); body.push_back((u8)f.h);
            Put16(body, gfx);
            Put16(body, (u32)(u16)(s16)f.x); Put16(body, (u32)(u16)(s16)f.y);
            for (const Piece& p : f.pieces)
            {
                Put16(body, (u32)(p.x & 0x1FF) | (u32)(p.y & 0x7F) << 9);
                Put16(body, (u32)p.size << 2 | (u32)p.shape << 4 | (u32)p.tile << 6);
            }
            gfx += 4 + (u32)f.tiles.size();
        }
        for (size_t a : at) Put16(d, (u32)(2 * frames.size() + a));
        d.insert(d.end(), body.begin(), body.end());
        return d;
    }
    Bytes Tiles() const
    {
        Bytes t;
        for (const Frame& f : frames)
        {
            Put32(t, (u32)f.tiles.size() << 8);
            t.insert(t.end(), f.tiles.begin(), f.tiles.end());
        }
        return t;
    }
    static Bytes Palette()
    {
        Bytes p;
        for (u32 i = 0; i < 16; i++) Put16(p, i | (31 - i) << 5 | (i * 2) << 10); // colour i: red i, green 31 - i, blue 2i
        return p;
    }
    static u32 Colour(u32 i) { return i ? Bgr555ToArgb((u16)(i | (31 - i) << 5 | (i * 2) << 10)) : 0; }
    // One 8 x 8 tile whose pixel (x, y) is colour c(x, y).
    template <class F> static Bytes Tile(F c)
    {
        Bytes t(32, 0);
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) t[y * 4 + x / 2] |= (u8)(c(x, y) << (x % 2 ? 4 : 0));
        return t;
    }
};

static void sprites()
{
    // Frame 0: an 8 x 8 piece at the frame's origin (pixel (x, y) in colour (x + y) % 16).
    // Frame 1: a 16 x 8 piece (two tiles) at the top left and an 8 x 8 one at (8, 8), both
    // placed from a frame origin of (-4, -4), in a 12 x 12 frame: the second is clipped.
    // Frame 2: a piece at the most negative positions (x -256, y -64) in a frame there.
    Sprite sp;
    const Bytes tileA = Sprite::Tile([](int x, int y) { return (x + y) % 16; });
    const Bytes tileB = Sprite::Tile([](int, int) { return 5; });
    const Bytes tileC = Sprite::Tile([](int x, int) { return x == 0 ? 0 : 9; });
    sp.frames.push_back({8, 8, 0, 0, {{0, 0, 0, 0, 0}}, tileA});
    Bytes two = tileB;
    two.insert(two.end(), tileC.begin(), tileC.end());
    two.insert(two.end(), tileA.begin(), tileA.end());
    sp.frames.push_back({12, 12, -4, -4, {{-4, -4, 1, 0, 0}, {4, 4, 0, 0, 2}}, two});
    sp.frames.push_back({8, 8, -256, -64, {{-256, -64, 0, 0, 0}}, tileB});
    const Bytes desc = sp.Descriptor(), tiles = sp.Tiles(), pal = Sprite::Palette();
    CHECK(SpriteFrames(desc) == 3);
    SpriteFrame f;
    CHECK(DecodeSpriteFrame(tiles, desc, pal, 0, f) && f.w == 8 && f.h == 8 && f.pieces == 1 && f.argb.size() == 64);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) CHECK(f.argb[y * 8 + x] == Sprite::Colour((x + y) % 16));
    CHECK(f.argb[0] == 0 && (f.argb[1] >> 24) == 0xFF);
    CHECK(DecodeSpriteFrame(tiles, desc, pal, 1, f) && f.w == 12 && f.h == 12 && f.pieces == 2);
    for (int y = 0; y < 12; y++)
        for (int x = 0; x < 12; x++)
        {
            u32 want = 0;
            if (y < 8 && x < 8) want = Sprite::Colour(5);
            else if (y < 8) want = x == 8 ? 0 : Sprite::Colour(9);
            else if (x >= 8) want = Sprite::Colour((x - 8 + y - 8) % 16);
            CHECK(f.argb[y * 12 + x] == want);
        }
    CHECK(DecodeSpriteFrame(tiles, desc, pal, 2, f) && f.w == 8 && f.argb[0] == Sprite::Colour(5) && f.argb[63] == Sprite::Colour(5));
    CHECK(!DecodeSpriteFrame(tiles, desc, pal, 3, f) && f.w == 0 && f.argb.empty());
    CHECK(!DecodeSpriteFrame(tiles, desc, pal, -1, f));
    // The palette's colours as melonDS shows them: 5 bits spread to 8.
    CHECK(Bgr555ToArgb(0x7FFF) == 0xFFFBFBFB && Bgr555ToArgb(0) == 0xFF000000 && Bgr555ToArgb(0x001F) == 0xFFFB0000 && Bgr555ToArgb(0x8000) == 0xFF000000);
    CHECK(Bgr555ToArgb(12 | 14 << 5 | 22 << 10) == 0xFF6171B2); // the game's frame colour
    std::vector<u32> colours;
    CHECK(ReadPalette(pal, colours) && colours.size() == 16 && colours[3] == Sprite::Colour(3));
    CHECK(ReadPalette(Bytes(512, 0), colours) && colours.size() == 256);
    CHECK(!ReadPalette(Bytes(30, 0), colours) && colours.empty());
    CHECK(!ReadPalette(Bytes(), colours));

    // Broken descriptors, tiles and palettes: refused, never a crash.
    auto refused = [&](const Bytes& d, const Bytes& t, const Bytes& p, int frame = 0) { SpriteFrame o; return !DecodeSpriteFrame(t, d, p, frame, o) && o.w == 0; };
    CHECK(!refused(desc, tiles, pal));
    CHECK(SpriteFrames(Bytes()) == 0 && SpriteFrames(Bytes(11, 1)) == 0);
    auto broken = [&](size_t at, u32 v, int bytes = 2) { Bytes d = desc; if (bytes == 2) W16(d, at, v); else d[at] = (u8)v; return d; };
    CHECK(SpriteFrames(broken(0, 0, 1)) == 0 && SpriteFrames(broken(1, 0, 1)) == 0); // no size
    CHECK(SpriteFrames(broken(2, 1)) == 0);                                        // the zero not zero
    CHECK(SpriteFrames(broken(6, 0)) == 0 && SpriteFrames(broken(6, 256)) == 0);     // no frame, too many
    CHECK(SpriteFrames(broken(6, 200)) == 0);                                      // a table past the end
    CHECK(refused(broken(12, 5000), tiles, pal));                                  // frame 0 past the end
    CHECK(refused(broken(12, (u32)desc.size() - 12 - 5), tiles, pal));             // its header cut
    const size_t f0 = 12 + 2 * 3;
    CHECK(refused(broken(f0, 0), tiles, pal) && refused(broken(f0, 65), tiles, pal));            // no piece, too many
    CHECK(refused(broken(f0, 2), tiles, pal));                                                   // a second piece read from the next frame's bytes: tiles past its chunk
    CHECK(refused(broken(f0 + 2, 0, 1), tiles, pal) && refused(broken(f0 + 3, 0, 1), tiles, pal)); // an empty frame
    CHECK(refused(broken(f0 + 4, (u32)tiles.size()), tiles, pal));                               // tiles past the resource
    CHECK(refused(broken(f0 + 4, 2), tiles, pal));                                               // tiles inside a header
    CHECK(refused(broken(f0 + 12, 3 << 4), tiles, pal));                                         // shape 3
    CHECK(refused(broken(f0 + 12, 1 << 2), tiles, pal));                                         // a 16 x 16 piece: more tiles than the frame's
    CHECK(refused(broken(f0 + 12, 1 << 6), tiles, pal));                                         // tile 1 of one
    CHECK(refused(broken(f0 + 12, 1023 << 6), tiles, pal));
    CHECK(refused(desc, Bytes(), pal) && refused(desc, Bytes(tiles.begin(), tiles.begin() + 20), pal));
    CHECK(refused(desc, tiles, Bytes(31, 0)) && refused(desc, tiles, Bytes(512, 0)) && refused(desc, tiles, Bytes()));
    {
        Bytes t = tiles;
        W32(t, 0, 31 << 8); // frame 0's chunk one byte short of its piece
        CHECK(refused(desc, t, pal));
        Bytes big = desc;
        big[0] = 0; // the descriptor's own size 0
        CHECK(SpriteFrames(big) == 0 && refused(big, tiles, pal));
    }

    // The same through the archive and the icon cache: frames decoded once and kept, scaled
    // copies through the kit's scaler (a whole factor: every pixel a block, the alpha kept),
    // a sprite that does not read remembered as such, nothing from a dropped archive.
    {
        std::vector<Bytes> resources(8);
        resources[4] = tiles; resources[5] = desc; resources[6] = pal;
        resources[7] = Bytes(30, 0); // not a palette
        RomData rom;
        CHECK(rom.Open(MakeRomFrom(resources).b) && rom.ResourceCount() == 8);
        CHECK(rom.SpriteFrames(5) == 3 && rom.SpriteFrames(4) == 0 && rom.SpriteFrames(9) == 0 && rom.SpriteFrames(-1) == 0);
        CHECK(rom.Sprite(4, 5, 6, 1, f) && f.w == 12 && f.pieces == 2);
        CHECK(!rom.Sprite(4, 5, 7, 1, f) && f.w == 0);
        CHECK(!rom.Sprite(5, 5, 6, 1, f) && !rom.Sprite(4, 6, 6, 1, f) && !rom.Sprite(4, 5, 6, 3, f));
        IconCache cache(&rom);
        const SpriteRef s = SpriteRef::Of(5);
        CHECK(s.tiles == 4 && s.desc == 5 && s.pal == 6);
        CHECK(cache.Frames(s) == 3 && cache.Frames(SpriteRef::Of(4)) == 0);
        const SpriteFrame* a = cache.Frame(s, 0);
        CHECK(a && a->w == 8 && a->argb[9] == Sprite::Colour(2));
        CHECK(cache.Frame(s, 0) == a);
        CHECK(!cache.Frame(s, 3) && !cache.Frame(s, -1) && !cache.Frame(SpriteRef::Of(4), 0) && !cache.Frame(SpriteRef::Of(4), 0));
        const SpriteFrame* big = cache.Scaled(s, 0, 3);
        CHECK(big && big->w == 24 && big->h == 24 && big->argb.size() == 576);
        for (int y = 0; y < 24 && big; y++)
            for (int x = 0; x < 24; x++) CHECK(big->argb[y * 24 + x] == a->argb[(y / 3) * 8 + x / 3]);
        CHECK(cache.Scaled(s, 0, 24, 24) == big && cache.Scaled(s, 0, 3) == big);
        const SpriteFrame* odd = cache.Scaled(s, 1, 30, 18); // not a whole factor: the size asked, nothing outside, the corners' alpha kept
        CHECK(odd && odd->w == 30 && odd->h == 18 && odd->argb.size() == 540 && odd->argb[0] == Sprite::Colour(5) && (odd->argb[17 * 30 + 29] >> 24) == 0xFF);
        CHECK(!cache.Scaled(s, 0, 0, 8) && !cache.Scaled(s, 0, 8, -1) && !cache.Scaled(s, 0, 0) && !cache.Scaled(s, 3, 2) && !cache.Scaled(SpriteRef::Of(9), 0, 2));
        IconCache none(nullptr);
        CHECK(none.Frames(s) == 0 && !none.Frame(s, 0) && !none.Scaled(s, 0, 2));
        rom.DropArchive();
        CHECK(rom.SpriteFrames(5) == 0 && !rom.Sprite(4, 5, 6, 0, f));
        IconCache dropped(&rom);
        CHECK(!dropped.Frame(s, 0));
        CHECK(cache.Frame(s, 0) == a); // what was decoded stays
    }
}

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

    compression();
    sprites();

    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("romdata_test: ok\n");
    return 0;
}
