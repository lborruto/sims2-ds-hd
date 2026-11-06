// The game's own data, read from the player's ROM at run time: its archive (rom.bin in the
// cartridge's file system, resources by id) and its strings in the five languages of the
// European game, as UTF-8. Nothing of the game is kept in this repository: everything here is
// read from the ROM the player gave, when it is needed.
#pragma once
#include <string>
#include <vector>

#include "common.h"

namespace melonDS { class NDS; }

namespace sims2wide
{
// A compressed resource: a 4-byte header (byte 0: bits 4-6 the kind, bit 7 a 16-bit delta
// undone after; bytes 1-3 the unpacked size), then the stream: kind 0 stored, 1 LZ10 (the
// DS BIOS's), 6 the game's own (an LZ with a table of run bytes, ported from the decoder the
// game keeps in its ITCM). `out` is the unpacked bytes, exactly the header's size; false
// (and `out` empty) for another kind, byte 0's low nibble set (never, in the game's
// compressed resources: a sprite descriptor 24 x 24 would otherwise read as LZ10), a stream
// that runs out, a copy from before the start, a run byte past the table, or a stream that
// ends anywhere but at the size. An unpacked size over kMaxUnpacked is refused.
constexpr size_t kMaxUnpacked = 4u << 20;
bool Decompress(const u8* data, size_t len, std::vector<u8>& out);
bool Decompress(const std::vector<u8>& data, size_t offset, std::vector<u8>& out);

// A BGR555 colour (bit 15 ignored) as 0xFFRRGGBB, the 5 bits spread as melonDS draws them
// ((c << 3) | (c >> 3): the game's #6171B2, #828ACB, #309A41 come out so), and a palette
// resource (32 bytes: 16 colours, or 512: 256) as such colours; false for another size.
u32 Bgr555ToArgb(u16 bgr);
bool ReadPalette(const std::vector<u8>& res, std::vector<u32>& argb);

// A sprite's frame as a picture: w x h pixels 0xAARRGGBB, straight alpha, the palette's
// colour 0 transparent (0) and every other opaque.
struct SpriteFrame
{
    int w = 0, h = 0;
    int pieces = 0; // the DS objects it was drawn from (0 for a scaled copy)
    std::vector<u32> argb;
};
// The game's sprites: a descriptor resource (u8 w, u8 h, u16 0, u16 ?, u16 frames, u32 tile
// bytes, u16 frame offsets from byte 12; each frame u16 pieces (1 to 64), u8 w, u8 h, u16 the
// byte offset of its compressed tiles in the tiles resource, s16 x, s16 y, then its pieces:
// u16 position (x 9 bits signed, y 7 bits signed), u16 attribute (bits 2-3 the size, 4-5 the
// shape, 6 and up the first tile, a count of 32-byte units into the unpacked tiles), the DS's OBJ sizes), a tiles resource (one
// compressed 4bpp tile stream per frame, Decompress) and a 16-colour palette resource. The
// frame's picture is w x h at (x, y): each piece's tiles are read in order (rows of 8 x 8,
// 32 bytes each, two pixels a byte, the low nibble first) and drawn at (piece x - x, piece y
// - y), clipped to the picture, colour 0 left transparent.
int SpriteFrames(const std::vector<u8>& desc); // 0: not a descriptor
bool DecodeSpriteFrame(const std::vector<u8>& tiles, const std::vector<u8>& desc, const std::vector<u8>& pal, int frame, SpriteFrame& out);

class RomData
{
public:
    // Finds rom.bin in the ROM's file system and keeps a copy of it; false when the ROM has no
    // such file (not this game) or its table does not fit.
    bool Open(const std::vector<u8>& rom);
    bool IsOpen() const { return !archive.empty(); }
    int ResourceCount() const { return offsets.empty() ? 0 : (int)offsets.size() - 1; }
    // The raw bytes of resource id (the archive's offset table: entry id to entry id + 1);
    // empty when id is out of the table.
    std::vector<u8> Resource(int id) const;

    // The strings of a language: 0 Japanese (reads the English file), 1 English, 2 French,
    // 3 German, 4 Italian, 5 Spanish, from the file the game's own table gives each; false
    // for another language or a file that does not read as a string table.
    bool LoadStrings(int language);
    int StringCount() const { return (int)strings.size(); }
    // The language the strings were loaded in, as the game's files have it: 1 English (also for
    // 0 Japanese, which reads the English file), 2 French, 3 German, 4 Italian, 5 Spanish; -1 none.
    int Language() const { return language; }
    int StringsFile() const { return stringsFile; } // the resource the strings came from (-1: none)
    // String id as UTF-8, its newlines kept: the game's 1-byte charset mapped (romdata.cpp),
    // an unknown byte shown as {xx}, a 2-byte escape (0xF0 and over) as {F0xx}; "" when id is
    // out of the table.
    std::string String(int id) const;
    // Frees the archive's copy (Resource() then finds nothing, Sprite() neither) and keeps the
    // strings loaded: for a program that wants the words alone. The running program keeps the
    // archive (App::rom): the icons are decoded from it as the menus need them (port/icons.h).
    void DropArchive() { archive.clear(); archive.shrink_to_fit(); offsets.clear(); offsets.shrink_to_fit(); }
    // A made-up string table (the checks' own words, never the game's) in `lang` (1-5), as
    // LoadStrings would leave it: what tests build the HD screens' compositions with.
    void SetStringsForTest(std::vector<std::string> made, int lang) { strings = std::move(made); language = lang; }

    // The game's sprites (below): how many frames the descriptor resource `desc` holds (0: not
    // a descriptor, or not in the archive), and frame `frame` of the sprite {tiles, desc, pal}
    // decoded (false when any of the three does not read).
    int SpriteFrames(int desc) const;
    bool Sprite(int tiles, int desc, int pal, int frame, SpriteFrame& out) const;

private:
    std::vector<u8> archive;
    std::vector<u32> offsets;         // the table, and the archive's size after it: resource i is [offsets[i], offsets[i + 1])
    std::vector<std::string> strings; // UTF-8
    int stringsFile = -1;
    int language = -1;
};

// A string from RomData::String without its escapes: the {xx} and {F0xx} it shows for a byte
// it has no meaning for (a brace group of up to 8 characters) dropped. The one dropper the HD
// text (status bar, menus, conversations) shares.
std::string DropEscapes(const std::string& in);

// The game's raw text bytes (a RAM buffer, e.g. a popup's) as UTF-8, as RomData::String decodes them.
std::string GameTextToUtf8(const std::string& raw);
// Whether the game's charset draws byte b as a letter (printable ASCII to 0x7A, or one of its
// accented letters and signs from 0x7B): what a name typed on its keyboard may hold (the French,
// German, Italian and Spanish keyboards have é è à î ë ê, ä ü ö Ä Ü Ö ß, ò à ù ì è é, ¡ ñ Ñ).
bool GameTextByte(unsigned char b);
// A text (UTF-8) with its capitals made small by the game's own pairs: A-Z, and each accented
// capital whose small letter the game's charset has (É / é, Ä / ä, Ñ / ñ…); everything else as is.
// (Typing the Sim's name: a letter found on the keyboard page of the other case.)
std::string GameLower(const std::string& utf8);

// The language the game shows: the firmware's user language (0 Japanese to 5 Spanish), which
// the game reads at start; any other value (Chinese, reserved) is taken as English.
// (romdata_nds.cpp: the only part that needs melonDS.)
int GameLanguage(melonDS::NDS& nds);
}
