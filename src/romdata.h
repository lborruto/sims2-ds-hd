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
    int StringsFile() const { return stringsFile; } // the resource the strings came from (-1: none)
    // String id as UTF-8, its newlines kept: the game's 1-byte charset mapped (romdata.cpp),
    // an unknown byte shown as {xx}, a 2-byte escape (0xF0 and over) as {F0xx}; "" when id is
    // out of the table.
    std::string String(int id) const;

private:
    std::vector<u8> archive;
    std::vector<u32> offsets;         // the table, and the archive's size after it: resource i is [offsets[i], offsets[i + 1])
    std::vector<std::string> strings; // UTF-8
    int stringsFile = -1;
};

// The language the game shows: the firmware's user language (0 Japanese to 5 Spanish), which
// the game reads at start; any other value (Chinese, reserved) is taken as English.
int GameLanguage(melonDS::NDS& nds);
}
