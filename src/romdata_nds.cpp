// The game's language from the DS: the only part of RomData that touches melonDS, kept apart
// from romdata.cpp so that tests/romdata_test.cpp links that file alone.
#include "romdata.h"

#include "NDS.h"

namespace sims2wide
{
int GameLanguage(melonDS::NDS& nds)
{
    // The firmware's user settings: bits 0-2 of the settings word (0 Japanese to 5 Spanish;
    // 6 Chinese and 7 are not in this game's table: English then).
    const int language = nds.GetFirmware().GetEffectiveUserData().Settings & 7;
    return language <= 5 ? language : 1;
}
}
