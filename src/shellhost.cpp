#include "shellhost.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>

#include "pads.h"
#include "port/ownwords.h"
#include "port/statusbar.h"
#include "romdata.h"
#include "shots.h"
#include "state.h"

namespace sims2wide
{
using W = OwnWordId;
using shell::Act;

static std::string Fill(std::string s, const std::string& v)
{
    if (size_t at = s.find("@1"); at != std::string::npos) s.replace(at, 2, v);
    return s;
}

static const char* PageName(shell::Page p)
{
    switch (p)
    {
    case shell::Page::Main: return "main";
    case shell::Page::Settings: return "settings";
    case shell::Page::Controls: return "controls";
    case shell::Page::Keys: return "keys";
    case shell::Page::States: return "states";
    case shell::Page::Launch: return "launch";
    }
    return "?";
}

static const char* AskName(shell::Ask a)
{
    switch (a)
    {
    case shell::Ask::None: return "none";
    case shell::Ask::Quit: return "quit";
    case shell::Ask::Load: return "load";
    case shell::Ask::Overwrite: return "overwrite";
    case shell::Ask::Message: return "message";
    }
    return "?";
}

void ShellHost::OpenMenu(App& app, const Input& input, shell::Page p)
{
    app.fast = false;
    app.micBlow = false;
    LoadSlots();
    Refresh(app, input);
    shell.Open(p);
    messageOnly = false;
    if (print) fprintf(stderr, "frame %ld: shell open page=%s\n", frame, PageName(p));
}

void ShellHost::Message(App& app, const Input& input, const std::string& text)
{
    OpenMenu(app, input, shell::Page::Main);
    model.message = text;
    messageOnly = true;
}

void ShellHost::Refresh(const App& app, const Input& input)
{
    model.language = app.rom && app.rom->Language() >= 1 ? std::clamp(app.rom->Language(), 1, 5) : shell::WordsLanguage(app.settings, autoLanguage);
    model.settings = const_cast<Settings*>(&app.settings);
    const Controls::Bindings& b = app.settings.controls;
    const bool labels = NintendoLabels();
    const SDL_GameControllerType type = input.pads.Type();
    for (int a = 0; a < Controls::COUNT; a++)
    {
        const int k = b.Key(a);
        model.keyGlyph[a] = k > 0 ? Controls::KeyName(k) : "";
        const int in = b.Pad(a);
        std::string g = in >= 0 ? Controls::InputKey(PrintedInput(in, input.padStyle, labels)) : "";
        if (g == "back" && input.padStyle == Controls::PLAYSTATION)
            g = type == SDL_CONTROLLER_TYPE_PS5 ? "create" : type == SDL_CONTROLLER_TYPE_PS3 ? "select" : "back";
        model.padGlyph[a] = g;
    }
    model.padFace = input.padStyle == Controls::PLAYSTATION ? ui::Face::PlayStation : input.padStyle == Controls::NINTENDO ? ui::Face::Nintendo : ui::Face::Xbox;
    model.keyboard = input.lastKeyboard;
    if (model.capturing >= 0 && !headless)
        model.captureLeft = (int)std::max<long>(0, ((long)captureUntil - (long)SDL_GetTicks() + 999) / 1000);
    model.statesOk = data.Ok();
    model.canUndo = canUndo && FileExists(data.Undo(code));
}

void ShellHost::LoadSlots()
{
    for (int i = 0; i < kStateSlots; i++)
    {
        model.used[i] = data.Ok() && FileExists(data.Slot(code, i));
        if (!model.used[i] || !ReadThumb(data.SlotThumb(code, i), model.slots[i])) model.slots[i] = Thumb();
    }
}

Thumb ShellHost::ThumbNow(const App& app, const Video& video) const
{
    const Composition c = video.Composed(app);
    int tw, th;
    const u32* top = app.TopImage(tw, th);
    return MakeThumb(RenderCompose(c, top, tw, th, app.Bottom(), 1), c.cw, c.ch, (long long)time(nullptr));
}

bool ShellHost::WriteSuspend(App& app, const Video& video)
{
    if (!data.Ok() || !app.nds) return false;
    const bool ok = SaveState(*app.nds, data.Suspend(code)) && WriteThumb(data.SuspendThumb(code), ThumbNow(app, video));
    std::ofstream(data.SuspendRom(code)) << romPath << "\n";
    if (print || headless) fprintf(stderr, "frame %ld: shell suspend %s\n", frame, ok ? "written" : "failed");
    return ok;
}

void ShellHost::Say(Video& video, const std::string& text) const
{
    if (headless || print) fprintf(stderr, "frame %ld: shell says %s\n", frame, text.c_str());
    if (!headless) video.Toast(text, 3000);
}

void ShellHost::SaveSettings(App& app) const
{
    if (!settingsPath.empty() && !app.settings.Save(settingsPath)) fprintf(stderr, "cannot write %s\n", settingsPath.c_str());
}

void ShellHost::ApplySetting(App& app, Video& video, int row)
{
    const Settings& s = app.settings;
    switch ((shell::Row)row)
    {
    case shell::Row::Layout: app.layout = s.layout; break;
    case shell::Row::Fullscreen: if (video.win) video.SetFullscreen(s.fullscreen); break;
    case shell::Row::Smooth: if (video.win) video.SetSmooth(s.smoothScaling); break;
    case shell::Row::Language:
        // The HD screens' words at once (the game's strings and the port's own); the game's
        // own text keeps the DS's language.
        if (app.rom && app.nds)
        {
            const int lang = s.language >= 1 && s.language <= 5 ? s.language : GameLanguage(*app.nds);
            if (app.rom->LoadStrings(lang)) app.hudStrings = HudStringsFrom(app.rom.get());
        }
        break;
    default: break; // (read where they are used, every frame)
    }
    SaveSettings(app);
}

void ShellHost::Do(App& app, Video& video, Input& input, const shell::Result& r)
{
    const int lang = model.language;
    auto log = [&](const char* what, int arg, bool ok) {
        if (print || headless) fprintf(stderr, "frame %ld: shell %s %d %s\n", frame, what, arg, ok ? "ok" : "failed");
    };
    switch (r.act)
    {
    case Act::None: break;
    case Act::Resume: shell.Close(); break;
    case Act::Quit:
        WriteSuspend(app, video);
        app.quit = true;
        shell.Close();
        break;
    case Act::SaveSlot:
    {
        const bool ok = data.Ok() && SaveState(*app.nds, data.Slot(code, r.arg)) && WriteThumb(data.SlotThumb(code, r.arg), ThumbNow(app, video));
        log("save", r.arg + 1, ok);
        LoadSlots();
        Say(video, ok ? Fill(OwnWordIn(lang, W::StSaved), std::to_string(r.arg + 1)) : OwnWordIn(lang, W::StFailed));
        break;
    }
    case Act::LoadSlot:
    {
        // The state of now first: the load's undo.
        const bool kept = SaveState(*app.nds, data.Undo(code));
        const bool ok = LoadState(*app.nds, data.Slot(code, r.arg));
        canUndo = kept && ok;
        input.lastMoney_ = -1;
        log("load", r.arg + 1, ok);
        Say(video, ok ? Fill(OwnWordIn(lang, W::StLoaded), std::to_string(r.arg + 1)) : OwnWordIn(lang, W::StFailed));
        if (ok) shell.Close();
        break;
    }
    case Act::Undo:
    {
        const bool ok = canUndo && LoadState(*app.nds, data.Undo(code));
        canUndo = false;
        input.lastMoney_ = -1;
        log("undo", 0, ok);
        Say(video, ok ? OwnWordIn(lang, W::StUndone) : OwnWordIn(lang, W::StFailed));
        if (ok) shell.Close();
        break;
    }
    case Act::Capture:
        input.StartCapture();
        model.capturing = r.arg;
        captureUntil = SDL_GetTicks() + 5000;
        model.captureLeft = 5;
        log("capture", r.arg, true);
        break;
    case Act::ClearBinding:
        if (model.keyboard) app.settings.controls.key[r.arg] = Controls::kNone;
        else app.settings.controls.pad[r.arg] = Controls::kNone;
        log("clear", r.arg, true);
        SaveSettings(app);
        break;
    case Act::ResetBindings:
        app.settings.controls.Reset();
        if (!headless) app.settings.controls.ResolveKeys();
        log("reset", 0, true);
        SaveSettings(app);
        break;
    case Act::Setting:
        log("setting", r.arg, true);
        ApplySetting(app, video, r.arg);
        break;
    default: break; // (the launch panel's: main.cpp's)
    }
}

void ShellHost::Frame(App& app, Video& video, Input& input, const shell::Input& in)
{
    const shell::Page page0 = shell.page;
    const shell::Ask ask0 = shell.ask;
    const int focus0 = shell.focus, askFocus0 = shell.askFocus;
    if (model.capturing >= 0)
    {
        // A binding being captured: the first key, or the button let go first; Esc or Start +
        // Back keep what it had; five seconds without anything too. (Headless: nothing to capture.)
        Settings& s = app.settings;
        const int a = model.capturing;
        bool done = false, cancelled = false;
        if (input.newKey != Controls::kDefault)
        {
            if (input.newKey == Controls::kNone) s.controls.key[a] = Controls::kNone;
            else s.controls.SetKey(a, input.newKey);
            done = true;
        }
        else if (input.newPad >= 0) { s.controls.SetPad(a, input.newPad); done = true; }
        else if (input.captureCancel || headless || SDL_GetTicks() > captureUntil) done = cancelled = true;
        if (done)
        {
            model.capturing = -1;
            input.capturing = false;
            if (!cancelled) SaveSettings(app);
            if (print || headless) fprintf(stderr, "frame %ld: shell captured %s\n", frame, cancelled ? "nothing" : "a binding");
            const shell::Result n = shell.CaptureDone(cancelled);
            if (n.act == Act::Capture) Do(app, video, input, n);
        }
        Refresh(app, input);
        return;
    }
    const shell::Result r = shell.Update(in, model);
    if (messageOnly && model.message.empty()) { shell.Close(); messageOnly = false; }
    Do(app, video, input, r);
    Refresh(app, input);
    if (print && shell.open && (shell.page != page0 || shell.ask != ask0 || shell.focus != focus0 || shell.askFocus != askFocus0))
        fprintf(stderr, "frame %ld: shell page=%s focus=%d ask=%s%s\n", frame, PageName(shell.page), shell.focus, AskName(shell.ask),
                shell.ask == shell::Ask::None ? "" : shell.askFocus ? " (yes)" : " (no)");
    if (print && !shell.open) fprintf(stderr, "frame %ld: shell closed\n", frame);
}

void ShellHost::CloseAsked(App& app, Video& video, Input& input)
{
    if (!shell.open)
    {
        OpenMenu(app, input);
        shell.AskQuit();
    }
    else if (shell.ask == shell::Ask::Quit) Do(app, video, input, {Act::Quit, 0});
    else
    {
        model.message.clear();
        model.capturing = -1;
        input.capturing = false;
        shell.AskQuit();
    }
}
}
