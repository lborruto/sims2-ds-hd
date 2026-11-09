// The HD shell's host (shell.h is the pure part): fills the shell's model from the program (the
// settings, the bindings as printed, the save states' files), carries out what the shell asks
// (save / load / undo a state, a binding captured or cleared, a setting applied and saved, the
// suspend state on quitting) and says what happened in a toast. The window and headless runs
// (driven by an input script's "pad" events: tests/console_check.sh) share it.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "NDS.h"
#include "app.h"
#include "input.h"
#include "shell.h"
#include "slots.h"
#include "video.h"

namespace sims2wide
{
struct ShellHost
{
    shell::Shell shell;
    shell::Model model;
    DataDir data;
    std::string code;          // the game code (the states' folder)
    std::string romPath;
    std::string settingsPath;  // "" headless: never saved there
    bool headless = false;
    bool print = false;        // --print-shell: what it does, on stderr
    bool messageOnly = false;  // opened for a message alone: closes with it
    bool canUndo = false;      // before-load.st exists (a load this session or an earlier one): read on start and after a write
    bool relaunch = false;     // Change game: back to the launch panel once the frame loop ends
    u64 romHeader = 0;         // the ROM's header hash (StateMeta: a state made with another file asks first)
    // Called before every state load (main.cpp: a copy of the game's save, as the backups make:
    // melonDS writes the cartridge's memory to disk on a load).
    std::function<void()> beforeLoad;
    std::function<void()> afterLoad;   // after every successful load (cancels queued taps and the held picture)
    unsigned captureUntil = 0; // SDL ticks
    long frame = 0;            // (for the log)
    int autoLanguage = 1;      // the words without the game's strings (the launch panel): the computer's
    std::string saveElsewhere; // the game's save is not beside the ROM (its folder is read-only): where it is (the main page's banner)

    bool Open() const { return shell.open; }
    // The pause menu opens on page p (the game stands still meanwhile).
    void OpenMenu(App& app, const Input& input, shell::Page p = shell::Page::Main);
    // A message over the game until A (the menu closes with it).
    void Message(App& app, const Input& input, const std::string& text);
    // The model's words, bindings and settings for this frame (the slots are read on opening).
    void Refresh(const App& app, const Input& input);
    void LoadSlots();
    // One frame of the open shell: its input (presses, the mouse in canvas pixels), what it asks.
    void Frame(App& app, Video& video, Input& input, const shell::Input& in);
    // The window was asked to close: the quit question (a second time: quit, keeping the place).
    void CloseAsked(App& app, Video& video, Input& input);
    // A short message: a toast in the window, a line on stderr headless.
    void Say(Video& video, const std::string& text) const;
    // A state load with the save's copy first (beforeLoad). A failed load puts back the machine of
    // a moment before, taken in memory (snapshot: given back to the caller, empty when it could not
    // be taken); when that fails too (restoreFailed) it is said on stderr, the caller toasts.
    bool Load(App& app, const std::string& path, std::vector<u8>* snapshot = nullptr);
    bool restoreFailed = false; // the last Load failed and the machine of before did not come back
    // F5 / F7: the quick state with its maker (F7 refuses another ROM file's, by a toast).
    bool QuickSave(App& app, Video& video);
    bool QuickLoad(App& app, Video& video);
    // The suspend state, its picture and its ROM (quitting).
    bool WriteSuspend(App& app, Video& video);
    // The composed picture of now, made small.
    Thumb ThumbNow(const App& app, const Video& video) const;

private:
    void Do(App& app, Video& video, Input& input, const shell::Result& r);
    void ApplySetting(App& app, Video& video, int row, bool save = true); // save: settings.ini written after
    // The settings file written, the window's size and place taken first (a window just switched
    // to or from fullscreen: taken before the switch, remember false).
    void SaveSettings(App& app, const Video& video, bool remember = true) const;
};
}
