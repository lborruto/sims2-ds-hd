// The HD shell's host (shell.h is the pure part): fills the shell's model from the program (the
// settings, the bindings as printed, the save states' files), carries out what the shell asks
// (save / load / undo a state, a binding captured or cleared, a setting applied and saved, the
// suspend state on quitting) and says what happened in a toast. The window and headless runs
// (driven by an input script's "pad" events: tests/console_check.sh) share it.
#pragma once
#include <string>

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
    bool canUndo = false;
    unsigned captureUntil = 0; // SDL ticks
    long frame = 0;            // (for the log)
    int autoLanguage = 1;      // the words without the game's strings (the launch panel): the computer's

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
    // The suspend state, its picture and its ROM (quitting).
    bool WriteSuspend(App& app, const Video& video);
    // The composed picture of now, made small.
    Thumb ThumbNow(const App& app, const Video& video) const;

private:
    void Do(App& app, Video& video, Input& input, const shell::Result& r);
    void ApplySetting(App& app, Video& video, int row);
    void SaveSettings(App& app) const;
};
}
