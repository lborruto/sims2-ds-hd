#include "slots.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdlib>

namespace sims2wide
{
namespace fs = std::filesystem;

Thumb MakeThumb(const std::vector<u8>& rgb, int w, int h, long long time)
{
    Thumb t;
    t.time = time;
    if (w <= 0 || h <= 0 || rgb.size() < (size_t)w * h * 3) return t;
    t.w = std::min(kThumbW, w);
    t.h = std::max(1, h * t.w / w);
    t.px.resize((size_t)t.w * t.h);
    for (int y = 0; y < t.h; y++)
        for (int x = 0; x < t.w; x++)
        {
            // The source pixels this one covers, averaged.
            const int x0 = x * w / t.w, x1 = std::max(x0 + 1, (x + 1) * w / t.w);
            const int y0 = y * h / t.h, y1 = std::max(y0 + 1, (y + 1) * h / t.h);
            unsigned r = 0, g = 0, b = 0, n = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++)
                {
                    const u8* p = &rgb[((size_t)yy * w + xx) * 3];
                    r += p[0];
                    g += p[1];
                    b += p[2];
                    n++;
                }
            t.px[(size_t)y * t.w + x] = 0xFF000000u | (r / n) << 16 | (g / n) << 8 | (b / n);
        }
    return t;
}

bool WriteThumb(const std::string& path, const Thumb& t)
{
    if (t.w <= 0 || t.h <= 0 || t.px.size() != (size_t)t.w * t.h) return false;
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    const u32 head[4] = {1, (u32)t.w, (u32)t.h, 0};
    const long long time = t.time;
    out.write("S2WT", 4);
    out.write((const char*)head, sizeof head);
    out.write((const char*)&time, sizeof time);
    out.write((const char*)t.px.data(), (std::streamsize)(t.px.size() * 4));
    return out.good();
}

bool ReadThumb(const std::string& path, Thumb& t)
{
    t = Thumb();
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    char magic[4];
    u32 head[4];
    long long time = 0;
    in.read(magic, 4);
    in.read((char*)head, sizeof head);
    in.read((char*)&time, sizeof time);
    if (!in || memcmp(magic, "S2WT", 4) != 0 || head[0] != 1 || head[1] == 0 || head[2] == 0 || head[1] > 1024 || head[2] > 1024) return false;
    Thumb r;
    r.w = (int)head[1];
    r.h = (int)head[2];
    r.time = time;
    r.px.resize((size_t)r.w * r.h);
    in.read((char*)r.px.data(), (std::streamsize)(r.px.size() * 4));
    if (!in) return false;
    t = std::move(r);
    return true;
}

std::string FormatTime(long long t)
{
    if (t <= 0) return "";
    const time_t tt = (time_t)t;
    struct tm lt {};
#ifdef _WIN32
    localtime_s(&lt, &tt);
#else
    localtime_r(&tt, &lt);
#endif
    char buf[32];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &lt);
    return buf;
}

std::string DataDir::GameDir(const std::string& code) const
{
    if (root.empty()) return "";
    std::string c;
    for (char ch : code)
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) c += ch;
    if (c.empty()) return "";
    const std::string dir = root + "states/" + c + "/";
    if (std::find(made_.begin(), made_.end(), dir) == made_.end())
    {
        std::error_code ec;
        fs::create_directories(fs::path(dir), ec);
        made_.push_back(dir);
    }
    return dir;
}

static std::string MadeDir(const std::string& root, const char* name)
{
    if (root.empty()) return "";
    const std::string dir = root + name + "/";
    std::error_code ec;
    fs::create_directories(fs::path(dir), ec);
    return dir;
}
std::string DataDir::Saves() const { return MadeDir(root, "saves"); }
std::string DataDir::Screenshots() const { return MadeDir(root, "screenshots"); }

bool CanWrite(const std::string& path)
{
    if (path.empty()) return false;
    if (FILE* f = fopen(path.c_str(), "rb"))
    {
        fclose(f);
        FILE* u = fopen(path.c_str(), "r+b");
        if (!u) return false;
        fclose(u);
        return true;
    }
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fclose(f);
    std::error_code ec;
    fs::remove(fs::path(path), ec);
    return true;
}

std::string SaveFallback(const DataDir& d, const std::string& savePath)
{
    const std::string dir = d.Saves();
    const std::string name = BaseName(savePath);
    return dir.empty() || name.empty() ? "" : dir + name;
}

std::string ShotName(long long t, int n)
{
    const time_t tt = (time_t)t;
    struct tm lt {};
#ifdef _WIN32
    localtime_s(&lt, &tt);
#else
    localtime_r(&tt, &lt);
#endif
    char buf[48];
    strftime(buf, sizeof buf, "sims2-%Y%m%d-%H%M%S", &lt);
    std::string s = buf;
    if (n > 1) s += "-" + std::to_string(n);
    return s + ".png";
}

// (A file in the game's folder; "" without one.)
static std::string In(const std::string& dir, const std::string& name) { return dir.empty() ? "" : dir + name; }
std::string DataDir::Slot(const std::string& code, int i) const { return In(GameDir(code), "slot" + std::to_string(i + 1) + ".st"); }
std::string DataDir::SlotThumb(const std::string& code, int i) const { return In(GameDir(code), "slot" + std::to_string(i + 1) + ".thumb"); }
std::string DataDir::SlotMeta(const std::string& code, int i) const { return In(GameDir(code), "slot" + std::to_string(i + 1) + ".meta"); }
std::string DataDir::Undo(const std::string& code) const { return In(GameDir(code), "before-load.st"); }
std::string DataDir::Suspend(const std::string& code) const { return In(GameDir(code), "suspend.st"); }
std::string DataDir::SuspendThumb(const std::string& code) const { return In(GameDir(code), "suspend.thumb"); }
std::string DataDir::SuspendMeta(const std::string& code) const { return In(GameDir(code), "suspend.meta"); }

u64 HashBytes(const u8* p, size_t n)
{
    u64 h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}

u64 HashFile(const std::string& path)
{
    if (path.empty()) return 0;
    std::ifstream in(fs::path(path), std::ios::binary);
    if (!in) return 0;
    std::vector<char> d((std::istreambuf_iterator<char>(in)), {});
    return HashBytes((const u8*)d.data(), d.size());
}

bool WriteMeta(const std::string& path, const StateMeta& m)
{
    if (path.empty()) return false;
    std::ofstream out {fs::path(path)};
    out << "rom=" << m.rom << "\nheader=" << m.header << "\nsave=" << m.save << "\n";
    return out.good();
}

bool ReadMeta(const std::string& path, StateMeta& m)
{
    m = StateMeta();
    if (path.empty()) return false;
    std::ifstream in {fs::path(path)};
    if (!in) return false;
    std::string line;
    while (std::getline(in, line))
    {
        while (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "rom") m.rom = v;
        else if (k == "header") m.header = strtoull(v.c_str(), nullptr, 10);
        else if (k == "save") m.save = strtoull(v.c_str(), nullptr, 10);
    }
    return !m.rom.empty();
}

bool FileExists(const std::string& path)
{
    if (path.empty()) return false;
    std::error_code ec;
    return fs::is_regular_file(fs::path(path), ec);
}

std::string BaseName(const std::string& path)
{
    const size_t s = path.find_last_of("/\\");
    return s == std::string::npos ? path : path.substr(s + 1);
}

std::vector<std::string> DataDir::Recents() const
{
    std::vector<std::string> r;
    if (root.empty()) return r;
    std::ifstream in(root + "recent.txt");
    std::string line;
    while (std::getline(in, line) && (int)r.size() < kRecents)
    {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (!line.empty() && FileExists(line) && std::find(r.begin(), r.end(), line) == r.end()) r.push_back(line);
    }
    return r;
}

void DataDir::PushRecent(const std::string& romPath) const
{
    if (root.empty() || romPath.empty()) return;
    std::vector<std::string> r = Recents();
    r.erase(std::remove(r.begin(), r.end(), romPath), r.end());
    r.insert(r.begin(), romPath);
    if ((int)r.size() > kRecents) r.resize(kRecents);
    std::ofstream out(root + "recent.txt");
    for (const std::string& p : r) out << p << "\n";
}

RomKind ClassifyHeader(const u8* h, size_t size, std::string* code)
{
    if (!h || size < 0x10) return RomKind::Unreadable;
    if (code) code->assign((const char*)h + 0x0C, 4);
    // The title (12 bytes, zero padded) and the code: The Sims 2 is "THE SIMS 2" and ASJ + region.
    if (memcmp(h, "THE SIMS 2", 10) != 0 || memcmp(h + 0x0C, "ASJ", 3) != 0) return RomKind::NotSims2;
    return h[0x0F] == 'P' ? RomKind::Sims2EU : RomKind::Sims2Other;
}

RomKind ClassifyRom(const std::string& path, std::string* code)
{
    std::ifstream in(fs::path(path), std::ios::binary);
    if (!in) return RomKind::Unreadable;
    u8 h[0x10] = {};
    in.read((char*)h, sizeof h);
    if (in.gcount() < (std::streamsize)sizeof h) return RomKind::Unreadable;
    return ClassifyHeader(h, sizeof h, code);
}
}
