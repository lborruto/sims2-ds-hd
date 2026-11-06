// The port's own words: what the HD screens say where the game's string table has no word (the
// minigames' controller hints and a few labels). One table, in the game's five languages (English,
// French, German, Italian, Spanish: the port's own translations, short, as they sit on chips),
// chosen by the language the game's strings were loaded in (RomData::Language: Japanese reads the
// English file, so English). "@1" is filled by the caller.
#pragma once

namespace sims2wide
{
class RomData;

enum class OwnWordId : int
{
    // The pointing games' pointer.
    Point, Slow,
    // Alien Autopsy: the tools (laser, hand, tweezers), what A does with each, the carry, the tear.
    AutopsyLaser, AutopsyHand, AutopsyTweezers,
    AutopsyCut, AutopsyLift, AutopsyPick,
    AutopsyToTray, AutopsyToFlask, AutopsyTear,
    AutopsyQuit, AutopsyQuitAgain,
    AutopsySpecimen, AutopsyOrgan, AutopsyFreed, AutopsyRemoved, AutopsyDestroyed, AutopsyParasites, AutopsyEating,
    // The Metal Detector.
    DetectorWalk, DetectorDig, DetectorDepth, DetectorPowerOff, DetectorCharge, DetectorSignal, DetectorNone,
    // The Art Gallery's painting: the hints, the tools 0..5, the brush sizes, the colour and page.
    ArtPaint, ArtTool, ArtNextColour, ArtUndo, ArtDone,
    ArtPencil, ArtEraser, ArtLine, ArtBucket, ArtRectangle, ArtFilledRectangle,
    ArtSmall, ArtMedium, ArtLarge,
    ArtColourOf, ArtPage,
    // Keelhaulin' Cards.
    CardsContinue, CardsChoose, CardsPickUp, CardsDraw, CardsDiscard,
    // The vacuum.
    VacuumDrive, VacuumHand, VacuumGrab, VacuumBlow, VacuumBlowNow,
    // The social reaction games.
    SocialAnswer, SocialAgain, SocialMatch,
    Count
};

// The word in the language rom's strings were loaded in (null rom, or none loaded: English).
const char* OwnWord(const RomData* rom, OwnWordId id);
// The word in language 1-5 (1 English .. 5 Spanish; any other: English).
const char* OwnWordIn(int language, OwnWordId id);
}
