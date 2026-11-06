// The HD minigame frame (milestone 6): every minigame keeps its playfield, the game's own art (it
// is the gameplay), shown big at its shape on the left (the touch screen 3x, the stylus's piece:
// the mouse plays it as the DS's stylus), the top screen (the world, or the game's own top part:
// the cards' score board) as a secondary panel at the top right, and around them an HD frame in
// the kit's look: a title plate with the minigame's name in the game's words, the HD status bar
// where the game keeps its HUD, and a side panel with the game's values from RAM (a well) and the
// controller's hints as the kit's chips, for the minigame's scheme (activities.h). What the game
// says changes the hints: the cards' quit question (A its "Yes", B its "No"), the end-of-turn
// message (A goes on), the vacuum's clog (the blow's hint lit red).
// The hints and the side panels' labels the game has no word for are the port's own, translated
// into the game's five languages (port/ownwords.*, by RomData::Language).
// Without the game's strings or RAM: milestone 1's activity layout (console.cpp, the 5 x 7 hints).
// With them but a minigame's own part of RAM not reading (its `valid` false): the frame stays
// (playfield, top screen, title, prompts as with nothing known) and the side panel is left out.
// The cards' help ("?" on the table, tapped by A on it or the mouse) sets only RAM's `box`: the
// deck's thumbnail is hidden under it and the prompts stay the table's (its pages are the game's
// own, in the playfield; not redrawn).
// Today: Keelhaulin' Cards, the vacuum and the social reaction games (their schemes from
// milestone 1: activities.*), and the pointing games (milestone 6): Alien Autopsy, the Metal
// Detector and the Art Gallery's painting, their pointer drawn over the playfield (a ring, gold
// while the stylus is down); the timing games (MooGoo Monkey, the Super Drencher, the crime game);
// the Scratch lottery (the ticket, its prize, each box's scratching, the game's result), the music
// mixer (the song, tempo, instrument, octave, tracks, what it does now) and its save page (the
// title typed, the three slots), and the wireless lobby (out of scope: a note, B its X).
#pragma once
#include <string>

#include "compose.h"
#include "console.h"
#include "gamestate.h"
#include "screens.h"

namespace sims2wide
{
class RomData;
class IconCache;
struct BlockMeasure;

// The game's strings: the titles (the cards' table "Keelhaulin' Cards" 492, the key item "Vacuum"
// 1968), the cards' board (the treasure chest 1684, "Cards" 1685, "Score" 1686, "Player" 2022,
// "Opponent" 2023), the buttons ("Quit" 61, "Yes" 48, "No" 49) and the stat panel's "Vacuum
// Earnings: $@1" 2014. The social reaction games have no name in the game's table that RAM
// tells (which social verb started one is not read): no title plate there.
constexpr int kStrMgCards = 492, kStrMgVacuum = 1968, kStrMgTreasure = 1684, kStrMgCardsCount = 1685, kStrMgScore = 1686;
constexpr int kStrMgPlayer = 2022, kStrMgOpponent = 2023, kStrMgQuit = 61, kStrMgYes = 48, kStrMgNo = 49, kStrMgVacuumEarnings = 2014;
// The pointing games: the titles (the lab's verb "Alien Autopsy" 1885, the key item "Metal
// Detector" 1970, the place "Art Gallery" 68), the money "$@1" 28, the detector's card "Found:"
// 1160 and its finds' names (two lines each from 1164 + 2 type; "¹" an empty line), the stat
// panel's "Metal Detector Earnings: $@1" 2015 (StatStatsLine 3); the painting's colours are
// resource 6028 (16 pages of 16 BGR555 colours, the swatch i the page's colour i + 1).
constexpr int kStrMgAutopsy = 1885, kStrMgDetector = 1970, kStrMgArtGallery = 68, kStrMgMoney = 28, kStrMgFound = 1160, kStrMgFind0 = 1164;
constexpr int kMgFindTypes = 9, kResArtColours = 6028;
// The timing games: the casino's table "Moogoo Monkey Cards" 493, the key item "Super Drencher"
// 1969, the weapon "Rat-A-Rang" 733; MooGoo's "Score" 1749, its fruit (1750 coconuts, 1751
// watermelons, 1752 pineapples: players 0..2) and "Player 1" 1758 (+ p); the Drencher's target's
// name (a Sim 323 + k, an object 463 + k: GameState's record index).
constexpr int kStrMgMooGoo = 493, kStrMgDrencher = 1969, kStrMgRatARang = 733, kStrMgMooScore = 1749, kStrMgMooFruit = 1750, kStrMgMooPlayer1 = 1758;
constexpr int kStrMgSimName0 = 323, kStrMgObjectName0 = 463;
constexpr int kStrMgDrencherObjectEnd = 822;   // (the objects' names end where their descriptions begin: --measure-menu measures them all)
constexpr int kDrencherPressureMax = 125, kDrencherPressureLow = 30;   // the gauge's top; under it the pump's hint is lit
// The Scratch lottery: the tickets' names (314 + tier: "Cash Mummy!", "Milking For Moo-lah", "The
// Emperor's Coffers"), the game's results ("Congratulations, you won $@1!" 317, "Sorry, better luck
// next time!" 318), its popups' "Close" 1866. The music mixer: "Music Mixer" 491, the songs (1770 +
// song), its buttons' own names (PLAY 1778, RECORD 1780, QUIT 1782, GENRE 1784, VOX 1788, PREP 1790,
// TEMPO 1794, TRACKS 1796), "Select Song" 1763, "The microphone is recording." 1764, "Do you want to
// save your song?" 1765, "Preview" 1766; the save page's "Song Title" 1804, "Burn Song to Cartridge"
// 1805 and the slots' "No Data" 11. The wireless lobby: "DS Wireless" 2026, "Exit" 2030.
constexpr int kStrMgTicket0 = 314, kStrMgWon = 317, kStrMgLost = 318, kStrMgClose = 1866;
constexpr int kStrMgMixer = 491, kStrMgMixSong0 = 1770, kStrMgMixPlay = 1778, kStrMgMixRecord = 1780, kStrMgMixQuit = 1782, kStrMgMixGenre = 1784;
constexpr int kStrMgMixVox = 1788, kStrMgMixPrep = 1790, kStrMgMixTempo = 1794, kStrMgMixTracks = 1796;
constexpr int kStrMgMixSelect = 1763, kStrMgMixMic = 1764, kStrMgMixSaveAsk = 1765, kStrMgMixPreview = 1766;
constexpr int kStrMgSongTitle = 1804, kStrMgBurn = 1805, kStrMgNoData = 11, kStrMgWireless = 2026, kStrMgExit = 2030;
constexpr int kStrMgMax = kStrMgExit;   // the largest id used

// The frame's geometry (canvas pixels). The playfield stays milestone 1's kActivityBottomTo (36,68,
// 768 x 576, framed), the secondary panel kActivityTopX/Y (384 wide); the title plate over the
// playfield's frame; the side panel under the secondary one (and the status bar), to the canvas's
// foot less a margin, as wide as the secondary panel's frame.
constexpr int kMgTitleX = 30, kMgTitleY = 8, kMgTitleH = 48, kMgTitleMaxW = kActivityBottomTo.w + 12;
constexpr float kMgTitleSize = 30, kMgTitleMin = 22;
// The side panel ends with the playfield's frame (650); the prompt bar (the controller's hints in a
// row, centred) runs under both, across the frame's width.
constexpr int kMgSideX = kActivityTopX - 6, kMgSideW = kActivityTopW + 12, kMgSideGap = 12;
constexpr int kMgSideBottom = kActivityBottomTo.y + kActivityBottomTo.h + 6;
constexpr int kMgPad = 14;                                    // the side panel's inner margin
constexpr int kMgInnerX = kMgSideX + kMgPad, kMgInnerW = kMgSideW - 2 * kMgPad;
constexpr Rect kMgPrompt {kMgTitleX, kMgSideBottom + 10, kMgSideX + kMgSideW - kMgTitleX, 52};
constexpr int kMgPromptPad = 20, kMgPromptGap = 34;           // the bar's margin; between two hints
constexpr float kMgHintSize = 24, kMgHintMin = 17;            // the hints' chips and labels
constexpr float kMgChipBig = 30;                              // the social answers' D-pad chips
constexpr int kMgAnswerRow = 46;                              // a social answer's row (its label a live crop 2x)
constexpr float kMgValueSize = 24, kMgValueMin = 16, kMgHeadSize = 18;
// The cards' deck on the touch screen (a thumbnail of the game's own: the values' well) and the
// social answers' labels (left, middle, right: the game's own words, live crops shown 2x).
constexpr Rect kMgDeckFrom {110, 6, 36, 53};
constexpr Rect kMgAnswerFrom[3] = {{6, 169, 82, 19}, {87, 150, 82, 19}, {168, 169, 82, 19}};

// Whether the HD frame draws screen s: an activity (Social, Cards, Vacuum, the pointing and timing games), the game's RAM read
// (game valid) and its strings there (rom with every id above).
bool MinigameFramed(ScreenState s, const GameState* game, const RomData* rom);

// The frame appended to c (empty): backdrop, the playfield and the secondary panel (pieces,
// framed), the title plate, the status bar (hud and strings given and ok, the game keeping its HUD:
// not the cards), the side panel (values, hints). bottom: the DS's touch screen (the live crops;
// null: none). The focus ring (the cards') is console.cpp's, over the playfield piece.
// icons: unused today (null fine); pointer: a pointing game's pointer (null or not shown: none).
// focus: the save page's (an index in its HotspotsFor: the slot it is on changes A's word).
void BuildMinigame(Composition& c, ScreenState s, int topW, int topH, const HudState* hud, const HudStrings* strings, const GameState& game,
                   const RomData& rom, const u32* bottom, IconCache* icons = nullptr, const PointerView* pointer = nullptr, int focus = -1);
// The pointer's layer (it moves every frame: nothing else rasters again).
constexpr int kMgPointerLayer = 6;

// What the activities' input needs from RAM (activities.h's ActivityRam) on the minigame up:
// the cards' question and message, the autopsy's tool and what it holds, the detector's slider,
// the painting's tool and swatch, and a question of the game's popup (its tick and cross where
// the game puts them: the painting's "Are you done painting?"). game null: nothing known.
ActivityRam MinigameRam(const GameState* game);

// (--measure-menu) every word the frame shows in its box: the titles, the board's labels and names,
// the values' line, the game's words on the hints.
void MeasureMinigames(const RomData& rom, BlockMeasure& m);
}
