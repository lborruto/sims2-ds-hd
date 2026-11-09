#include "shellhost.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>

#include "pads.h"
#include "port/ownwords.h"
#include "port/statusbar.h"
#include "host.h"
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
    case shell::Ask::ResetTab: return "reset";
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
    model.canUndo = canUndo; // (no file look-up a frame: kept as the writes leave it)
    model.canOpenFolder = !headless && data.Ok();
    model.banner = saveElsewhere.empty() ? "" : Fill(OwnWordIn(model.language, W::SaveElsewhere), saveElsewhere);
}

void ShellHost::LoadSlots()
{
    for (int i = 0; i < kStateSlots; i++)
    {
        model.used[i] = data.Ok() && FileExists(data.Slot(code, i));
        if (!model.used[i] || !ReadThumb(data.SlotThumb(code, i), model.slots[i])) model.slots[i] = Thumb();
        // Made with another ROM file (another path, or another header): the load says so.
        StateMeta meta;
        model.otherRom[i].clear();
        if (model.used[i] && ReadMeta(data.SlotMeta(code, i), meta) && (meta.rom != romPath || meta.header != romHeader)) model.otherRom[i] = BaseName(meta.rom);
    }
}

Thumb ShellHost::ThumbNow(const App& app, const Video& video) const
{
    const Composition c = video.Composed(app);
    int tw, th;
    const u32* top = app.TopImage(tw, th);
    return MakeThumb(RenderCompose(c, top, tw, th, app.Bottom(), 1), c.cw, c.ch, (long long)time(nullptr));
}

bool ShellHost::Load(App& app, const std::string& path, std::vector<u8>* snapshot)
{
    restoreFailed = false;
    if (beforeLoad) beforeLoad();
    // The machine as it is, in memory: a load that fails half-way (a broken or cut file) puts it
    // straight back, so a half-read state is never played on (slots, Undo, F7, Continue alike).
    std::vector<u8> local;
    std::vector<u8>& before = snapshot ? *snapshot : local;
    before.clear();
    const bool kept = SaveStateTo(*app.nds, before);
    if (!kept) before.clear();
    if (LoadState(*app.nds, path))
    {
        if (afterLoad) afterLoad();
        return true;
    }
    const bool back = kept && LoadStateFrom(*app.nds, before);
    if (!back)
    {
        // Nothing to go back to, or the way back failed too: said always (the caller's toast).
        restoreFailed = true;
        fprintf(stderr, "frame %ld: shell restore failed after a failed load of %s (%s)\n", frame, path.c_str(),
                kept ? "the state of before did not load" : "the state of before could not be taken");
    }
    else if (print || headless) fprintf(stderr, "frame %ld: shell restore ok\n", frame);
    before.clear();
    return false;
}

bool ShellHost::QuickSave(App& app, Video& video)
{
    // The state first (beside it, then in its place: a failed write keeps the old one and its
    // maker), then its maker in the old one's place; a maker that cannot be written goes (never
    // the old one's over the new state).
    bool ok = SaveState(*app.nds, data.Quick(code));
    if (ok && !WriteMeta(data.QuickMeta(code), {romPath, romHeader, 0}))
    {
        std::error_code ec;
        std::filesystem::remove(data.QuickMeta(code), ec);
        ok = false;
    }
    Say(video, OwnWordIn(model.language, ok ? W::ToastStateSaved : W::StFailed));
    return ok;
}

bool ShellHost::QuickLoad(App& app, Video& video)
{
    // F7 asks nothing: a state another ROM file made is refused, said (F5 saves over it).
    StateMeta meta;
    if (ReadMeta(data.QuickMeta(code), meta) && (meta.rom != romPath || meta.header != romHeader))
    {
        std::string msg = OwnWordIn(model.language, W::QuickOtherRom);
        if (size_t at = msg.find("@1"); at != std::string::npos) msg.replace(at, 2, BaseName(meta.rom));
        if (print || headless) fprintf(stderr, "frame %ld: shell quick refused: another file\n", frame);
        Say(video, msg);
        return false;
    }
    const bool ok = FileExists(data.Quick(code)) && Load(app, data.Quick(code));
    Say(video, OwnWordIn(model.language, ok ? W::ToastStateLoaded : FileExists(data.Quick(code)) ? W::StFailed : W::ToastNoState));
    return ok;
}

bool ShellHost::WriteSuspend(App& app, Video& video)
{
    if (!data.Ok() || !app.nds) return false;
    // The state first (a failed write keeps whatever was there, with its maker), then its picture
    // and its maker (the save's hash as it is now: a save written later makes the place stale).
    // A picture or a maker that cannot be written fails the suspend: its files go (nothing is
    // offered as Continue that its maker does not describe).
    const bool saved = SaveState(*app.nds, data.Suspend(code));
    const bool ok = saved && WriteThumb(data.SuspendThumb(code), ThumbNow(app, video)) &&
                    WriteMeta(data.SuspendMeta(code), {romPath, romHeader, HashFile(Host::SavePath)});
    if (saved && !ok)
    {
        std::error_code ec;
        for (const std::string& f : {data.Suspend(code), data.SuspendThumb(code), data.SuspendMeta(code)}) std::filesystem::remove(f, ec);
    }
    if (print || headless) fprintf(stderr, "frame %ld: shell suspend %s\n", frame, ok ? "written" : "failed");
    if (!ok) Say(video, OwnWordIn(model.language, W::StFailed));
    return ok;
}

void ShellHost::Say(Video& video, const std::string& text) const
{
    if (headless || print) fprintf(stderr, "frame %ld: shell says %s\n", frame, text.c_str());
    if (!headless) video.Toast(text, 3000);
}

void ShellHost::SaveSettings(App& app, const Video& video, bool remember) const
{
    if (settingsPath.empty()) return;
    if (remember) video.RememberSize(app.settings);
    if (!app.settings.Save(settingsPath)) fprintf(stderr, "cannot write %s\n", settingsPath.c_str());
}

void ShellHost::ApplySetting(App& app, Video& video, int row, bool save)
{
    const Settings& s = app.settings;
    switch ((shell::Row)row)
    {
    case shell::Row::Layout: app.layout = s.layout; break;
    case shell::Row::Fullscreen:
        if (video.win)
        {
            video.RememberSize(app.settings); // (the window's, before it fills the screen)
            video.SetFullscreen(s.fullscreen);
        }
        if (save) SaveSettings(app, video, false);
        return;
    case shell::Row::Scaling: if (video.win) video.SetScaling(s.scaling); break;
    // (The language: the DS's firmware and the strings are chosen when the game starts; the row's
    // note says so. The battery saver, the volume, mute and the rest are read where they are used.)
    default: break; // (read where they are used, every frame)
    }
    if (save) SaveSettings(app, video);
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
    case Act::ChangeGame:
        // As Quit (the place kept), then the launch panel (main.cpp starts again from it).
        WriteSuspend(app, video);
        app.quit = true;
        relaunch = true;
        log("change", 0, true);
        shell.Close();
        break;
    case Act::OpenFolder:
    {
        // A file URL of the folder (forward slashes; a Windows drive after a third slash).
        std::string p = data.root;
        std::replace(p.begin(), p.end(), '\\', '/');
        const std::string url = "file://" + std::string(p.empty() || p[0] != '/' ? "/" : "") + p;
        const bool ok = SDL_OpenURL(url.c_str()) == 0;
        log("folder", 0, ok);
        if (!ok) Say(video, OwnWordIn(lang, W::StFailed));
        break;
    }
    case Act::SaveSlot:
    {
        // The state first (beside it, then in its place: a failed write keeps the old slot, its
        // picture and its maker), then its picture and its maker in their places; a maker that
        // cannot be written goes (never the old one's over the new state: no warning, it loads).
        const bool saved = data.Ok() && SaveState(*app.nds, data.Slot(code, r.arg));
        const bool thumb = saved && WriteThumb(data.SlotThumb(code, r.arg), ThumbNow(app, video));
        const bool meta = saved && WriteMeta(data.SlotMeta(code, r.arg), {romPath, romHeader, 0});
        std::error_code ec;
        if (saved && !thumb) std::filesystem::remove(data.SlotThumb(code, r.arg), ec); // (never the old place's picture)
        if (saved && !meta) std::filesystem::remove(data.SlotMeta(code, r.arg), ec);
        const bool ok = saved && thumb && meta;
        log("save", r.arg + 1, ok);
        LoadSlots();
        Say(video, ok ? Fill(OwnWordIn(lang, W::StSaved), std::to_string(r.arg + 1)) : OwnWordIn(lang, W::StFailed));
        break;
    }
    case Act::LoadSlot:
    {
        // The state of now, in memory: the load's undo, written as before-load.st only once the load
        // went well. A load that fails is undone by Load itself (that same state, never an older
        // before-load.st) and before-load.st and Undo stay exactly as they were.
        std::vector<u8> before;
        const bool ok = Load(app, data.Slot(code, r.arg), &before);
        if (ok) canUndo = !before.empty() && WriteStateBytes(before.data(), before.size(), data.Undo(code));
        input.lastMoney_ = -1;
        log("load", r.arg + 1, ok);
        Say(video, ok ? Fill(OwnWordIn(lang, W::StLoaded), std::to_string(r.arg + 1)) : OwnWordIn(lang, W::StFailed));
        if (ok) shell.Close();
        break;
    }
    case Act::Undo:
    {
        const bool ok = canUndo && Load(app, data.Undo(code));
        canUndo = !ok && canUndo;
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
        SaveSettings(app, video);
        break;
    case Act::ResetBindings:
        app.settings.controls.Reset();
        if (!headless) app.settings.controls.ResolveKeys();
        log("reset", 0, true);
        SaveSettings(app, video);
        break;
    case Act::Setting:
        log("setting", r.arg, true);
        ApplySetting(app, video, r.arg);
        break;
    case Act::ResetTab:
        // The tab's rows went back to their defaults: each applied (and saved) as if changed.
        log("reset tab", r.arg, true);
        // (One write after them all, without the window's size: a fullscreen change may have just
        // gone back to windowed.)
        for (shell::Row row : shell::TabRows(r.arg).rows) ApplySetting(app, video, (int)row, false);
        SaveSettings(app, video, false);
        break;
    default: break; // (the launch panel's: main.cpp's)
    }
}

void ShellHost::Frame(App& app, Video& video, Input& input, const shell::Input& in)
{
    const shell::Page page0 = shell.page;
    const shell::Ask ask0 = shell.ask;
    const int focus0 = shell.focus, askFocus0 = shell.askFocus, tab0 = shell.tab;
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
            if (!cancelled) SaveSettings(app, video);
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
    if (print && shell.open && (shell.page != page0 || shell.ask != ask0 || shell.focus != focus0 || shell.askFocus != askFocus0 || shell.tab != tab0))
        fprintf(stderr, "frame %ld: shell page=%s%s focus=%d ask=%s%s\n", frame, PageName(shell.page),
                shell.page == shell::Page::Settings ? (" tab=" + std::to_string(shell.tab)).c_str() : "", shell.focus, AskName(shell.ask),
                shell.ask == shell::Ask::None ? "" : shell.askFocus ? " (yes)" : " (no)");
    if (print && !shell.open) fprintf(stderr, "frame %ld: shell closed\n", frame);
}

void ShellHost::CloseAsked(App& app, Video& video, Input& input)
{
    messageOnly = false;
    shell.sequence = false;
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
