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
    // Keelhaulin' Cards (Continue: generic, a conversation's A too).
    Continue, CardsChoose, CardsPickUp, CardsDraw, CardsDiscard,
    // The vacuum.
    VacuumDrive, VacuumHand, VacuumGrab, VacuumBlow, VacuumBlowNow,
    VacuumTake, VacuumTaking, VacuumNoFind,
    AutopsyTakeOrgan, AutopsyTakeParasite,
    // The social reaction games.
    SocialAnswer, SocialAgain, SocialMatch,
    // MooGoo Monkey: the hints, the round and the deck.
    MooGooPlay, MooGooChooseMonkey, MooGooBet, MooGooPlayCard, MooGooUndoBet, MooGooPlayOn, MooGooTakeBack,
    MooGooPlayers, MooGooConfirm, MooGooRound, MooGooCardsLeft,
    // The Super Drencher.
    DrencherFire, DrencherPump, DrencherTarget, DrencherRefill, DrencherRun, DrencherPutAway, DrencherWater, DrencherPressure,
    DrencherNone,
    // The Ratticator's crime game (its run and put away: the Drencher's words).
    CrimeScare, CrimeAim, CrimeThrow, CrimeSubdue, CrimeReady, CrimeAimLabel, CrimeHit,
    // The Scratch lottery.
    ScratchHold, ScratchRub, ScratchQuitAgain, ScratchTopPrize, ScratchOpened,
    // The music mixer and its save page.
    MixerTouch, MixerPoint, MixerOctave, MixerSustain, MixerStop, MixerVox, MixerVoxStop, MixerChoose, MixerBack,
    MixerSong, MixerInstrument, MixerReady, MixerPlaying, MixerRecording, MixerHelp,
    SaveType, SaveHere, SaveOverwrite, SaveErase, SaveSlot,
    // The wireless lobby.
    TradingNote,
    // A help text, a card: closed.
    Close,
    // (ux task 4c) A word on every hint chip: the D-pad moving a list's or a question's focus, the
    // stat panel's pages, a slider's value, the concept art opened, Create-a-Sim's model turned; a
    // list the D-pad scrolls where A selects (Choose would say A's word twice: Wählen, Choisir).
    HintChoose, HintPages, HintAdjust, HintView, HintTurn, HintScroll,
    // The town's hint for the HD menu (its chip says the button).
    Menu,
    // A controller connected (@1 its name, @2 its pause buttons, Pads' PauseCombo): the window's toast.
    PadConnected,
    // The pause menu, beside Start + Back's chips (the cursor's hint).
    Pause,
    // The painting's next palette page; a minigame's help ("?"): what a button does, leaving it;
    // the stick cursor (the cursor's hint).
    ArtNextPalette, HelpWhat, HelpLeave, Cursor,
    // The menus: a shop cell's A (it selects; Pay buys), the bin armed (X again throws the item
    // away), an empty Goals / Interact tab.
    Select, TrashAgain, NoGoals, NoActions,
    // MooGoo's line for a bet (the game's says "Touch a Monkey!").
    MooGooBetLine,
    // The HD shell (shell.*): the pause menu, its pages and questions, the settings' rows and
    // values, the controls card, the bindings, the save states, the toasts, the launch panel.
    ShellPaused, ShellResume, ShellSettings, ShellControls, ShellStates, ShellQuit, ShellBack, ShellChange, ShellOpen,
    ShellYes, ShellNo, ShellOk, ShellOn, ShellOff, ShellPauseMenu,
    SetLayout, SetFullscreen, SetScaling, SetRes3D, SetWide, SetFps60, SetVolume, SetFastSound, SetStickCursor, SetDeadZone,
    SetTrigger, SetRumble, SetMic, SetLanguage, MicBlowKey, LangAuto, LangNote,
    LayoutConsole, LayoutAuto, LayoutFocus, LayoutSide, LayoutStacked,
    CtlWalk, CtlTouch, CtlBack, CtlZoom, CtlCamera, CtlFast, CtlKeys, CtlBlow,
    ActRight, ActLeft, ActUp, ActDown, ActSwap, ActSaveState, ActLoadState, ActBlow, ActTouch,
    KeysPress, KeysCancel, KeysClear, KeysAll, KeysReset, KeysKeyboard, KeysController,
    StEmpty, StSave, StLoad, StLoadAsk, StOverwriteAsk, StUndo, StSaved, StLoaded, StUndone, StFailed, StNoFolder,
    QuitAsk, QuitNote,
    ToastStateSaved, ToastStateLoaded, ToastNoState, ToastShot, ToastBackup, ToastCapture, ToastNotSaving, NotEU,
    LaunchContinue, LaunchWhere, LaunchChoose, LaunchRecent, LaunchDrop, LaunchWrong, LaunchUnreadable, LaunchStart, LaunchTitle,
    WirelessNote,
    // The review's round: change game, open the saves' folder, a stale place, a state from another
    // file, a place that did not load.
    ShellChangeGame, ShellOpenFolder, LaunchSaveNewer, StOtherRom, LaunchContinueFailed, QuickOtherRom,
    // Platform (task 5): the settings' new rows and values, their notes, the save moved to the
    // settings folder (the pause menu's banner), a window screenshot's toast.
    SetBattery, SetMute, SetBackground, SetFastSpeed, FastMax, SetFastKey, FastHold, FastPress, SetFastBoot, SetHidePointer,
    SetCursorSpeed, SetLeftHanded, ScaleSharp, ScaleSmooth, ScaleNearest,
    BatteryNote, BackgroundNote, FastBootNote, ScalingNote, SaveElsewhere, ToastShotAt,
    // The save back beside the ROM (its folder writable again) from the settings folder's copy.
    SaveMovedBack,
    // The interface's size and the game's pictures (task 10): the rows, Auto, their notes.
    SetUiSize, UiAuto, UiSizeNote, SetPictures, PicturesNote,
    // Settings in five tabs (ux task 5): the tabs' names (Controls: ShellControls), the Advanced
    // heading, a note for every row that had none, the real microphone, Language's next-start tag,
    // Battery saver's hold on its rows, the reset question (@1 the tab) and its hint.
    TabDisplay, TabGraphics, TabSound, TabGame, SetAdvanced,
    LayoutNote, FullscreenNote, Res3DNote, WideNote, Fps60Note, VolumeNote, MuteNote, FastSoundNote, FastSpeedNote,
    FastKeyNote, HidePointerNote, StickCursorNote, CursorSpeedNote, DeadZoneNote, LeftHandedNote, TriggerNote,
    RumbleNote, MicNote, KeysNote,
    MicReal, NextStart, SaverHolds, ResetTabAsk, ResetTab,
    // The world's prompt (port/prompt.*): A held shows the other actions, Up / Down choose one
    // while it is held, Left / Right switch to the next target near the hero.
    PromptMore, PromptOther,
    // (ux task 6, the front end and the minigames) The mixer's X: play the song (the game's word is
    // its button's, START in German).
    MixerPlay,
    Count
};

// The word in the language rom's strings were loaded in (null rom, or none loaded: English).
const char* OwnWord(const RomData* rom, OwnWordId id);
// The word in language 1-5 (1 English .. 5 Spanish; any other: English).
const char* OwnWordIn(int language, OwnWordId id);
}
