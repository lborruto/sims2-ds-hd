// The phone (CCellphoneMenu) in HD: an HD phone panel over the dimmed world, every page in the
// game's words, driven by the game's own buttons. The game's LCD look (a green screen, dark
// letters, the highlighted row a dark bar) with its header (the signal bars, the game's sprite
// 2419; the date "@2/@1" 0x83C and the clock as the status bar writes it), and below it the
// game's own Up / OK / Down buttons (sprites 8448 / 5806 / 2755, fitted) and its X (3068, at ui::kTierS).
//
// Pages (GameState::town.phone): 0 the menu ("Messages-@1" with the inbox's count, "FULL" at
// ten; Phonebook, Options, Exit: the game's table); 2 the messages (the senders, an unread one
// marked as the game's "*" does, a dot; Exit); 3 a message (its sender, its text with @1..@4
// filled as the game fills them, "Next Message", "Delete Message", Exit); 4 the ringtones
// ("Ringtone @1" 0..3, the three recorded slots "Empty"; the current one marked with a dot, as an unread message is); 5 the
// phonebook (Call Friend, "Call Maid - $@1" with the game's price, Prank Call, Exit); 7 the
// contacts (the Sims the game lists; "You Have No Contacts" when none); 6 / 8 / 9 the answers
// (the maid's, a prank's, a friend's: who and what, Exit); 1 an incoming call (who and what,
// Exit); 10 leaving (the phone alone). The rows the HD panel shows: up to kPhRows around the
// game's own (its scroll), the game's highlight held as the focus (MenuView::selected).
//
// What performs: the game's own buttons, its DS keys (the phone reads Up, Down and A itself:
// research m3 §4): Up / Down press the DS's Up / Down as many times as the game's highlight must
// move to reach the next HD row (a message's text lines are rows to the game: one press of Down
// from the sender goes past them all to "Next Message"); A moves the highlight to the focused
// row the same way if it is not there, then presses the DS's A (the game's OK). B goes back one
// level where the page has the game's Exit row (the highlight to Exit, then A: a message to the
// list, a list to the menu), else taps the game's X (kPhoneClose: the menu, hung up); MENU taps
// the X from any page (MenuView::menu). Mouse: the HD Up / OK / Down and X map onto the game's own buttons.
//
// A page whose content does not read as the game's (the contacts' list not matching the game's
// count or highlighted Sim, a recorded custom ringtone whose name is not decoded) shows the
// game's own LCD instead, a live crop 2x in the HD phone: the same buttons drive it.
#pragma once
#include "compose.h"
#include "gamestate.h"
#include "port/menuview.h"
#include "screens.h"

namespace sims2hd
{
class RomData;
class IconCache;
struct BlockMeasure;
struct HudStrings;

// The game's touch points (DS bottom pixels): its Up, OK and Down buttons.
constexpr int kPhUpX = 43, kPhOkX = 127, kPhDownX = 212, kPhBtnY = 166;
// The game's LCD on the touch screen (for the fallback's live crop): its text rows.
constexpr Rect kPhLcdFrom {36, 14, 184, 112};

// The game's strings and sprites.
constexpr int kStrPhExit = 0x816, kStrPhRingtone = 0x817, kStrPhEmpty = 0x818, kStrPhDelete = 0x819, kStrPhNext = 0x81A;
constexpr int kStrPhNoContacts = 0x81B, kStrPhMaid = 0x81D, kStrPhMaidName = 0x823, kStrPhCall = 0x824, kStrPhAway = 0x825;
constexpr int kStrPhMood = 0x826, kStrPhFull = 0x813, kStrPhDate = 0x83C, kStrPhPlace = 0x43;
constexpr int kStrPhPhonebook[4] = {0x81C, 0x81D, 0x81E, 0x816};
constexpr int kSprPhSignal = 2419, kSprPhUp = 8448, kSprPhOk = 5806, kSprPhDown = 2755, kSprPhX = 3068;

// HD geometry (canvas).
constexpr Rect kPhPanel {350, 18, 580, 630};
constexpr Rect kPhLcd {380, 92, 520, 420};
constexpr Rect kPhHeader {kPhLcd.x + 28, kPhLcd.y + 10, kPhLcd.w - 56, 44};
constexpr Rect kPhList {kPhLcd.x + 16, kPhLcd.y + 60, kPhLcd.w - 32, kPhLcd.h - 72};
constexpr int kPhRowH = 48, kPhRows = 7;
// Frames a contacts page of 0 rows that disagrees with the records is waited out (the game builds its list) before the crop.
constexpr int kPhContactsGrace = 8;
constexpr Rect kPhX {kPhPanel.x + kPhPanel.w - 84, kPhPanel.y + 18, 56, 56};
constexpr Rect kPhTitle {kPhPanel.x + 40, kPhPanel.y + 18, kPhPanel.w - 140, 56};
constexpr Rect kPhUp {400, 522, 120, 84}, kPhOk {586, 528, 108, 72}, kPhDown {760, 522, 120, 84};
constexpr u32 kPhRowId = kModPhone + 0x800;
// The header's date and clock have a still layer of their own (a minute turning rasters it alone, not the phone).
constexpr int kPhClockLayer = 3;

// The phone's rows on its page: the game's row of each (its highlight's index), the words.
struct PhoneRow
{
    int row = -1;
    std::string text;
    bool unread = false;       // messages: the game's "*" (not read yet)
    bool marked = false;       // ringtones: the current one
};
struct PhonePage
{
    bool valid = false;
    std::string title;         // a message's or an answer's: who (its sender, the Sim called)
    std::string body;          // its text
    std::vector<PhoneRow> rows;
    bool crop = false;         // the game's own LCD instead (its content did not read)
};
PhonePage PhonePageOf(const GameState& game, const RomData& rom);

// The view (valid on Phone when RAM's phone reads and the game's strings are there).
MenuView PhoneView(ScreenState s, const GameState* game, const RomData* rom);
MenuView PhoneViewOf(ScreenState s, const GameState& game, const RomData* rom);
// The HD phone appended to c (after the dimmed world).
void BuildPhone(Composition& c, const MenuView& v, int focus, const GameState& game, const RomData& rom, IconCache* icons, const u32* bottom,
                const HudStrings* strings);

// (A check, --measure-menu) the phone's words in their rows and boxes.
void MeasurePhone(const RomData& rom, BlockMeasure& m);
}
