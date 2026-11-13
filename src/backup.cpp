#include "backup.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace sims2hd
{
namespace fs = std::filesystem;
static constexpr unsigned kEvery = 30 * 60 * 1000; // ms
static constexpr int kKeep = 8;

static std::vector<char> ReadAll(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    return std::vector<char>((std::istreambuf_iterator<char>(in)), {});
}

void Backups::Start(const std::string& path)
{
    savePath = path;
    lastTick = 0;
    const std::string what = Copy();
    if (!what.empty()) fprintf(stderr, "save backup: %s\n", what.c_str());
}

bool Backups::Tick(unsigned now)
{
    if (now - lastTick < kEvery) return false;
    lastTick = now;
    const std::string what = Copy();
    if (!what.empty()) fprintf(stderr, "save backup: %s\n", what.c_str());
    return !what.empty() && what.rfind("cannot ", 0) != 0;
}

std::string Backups::Copy() const
{
    if (savePath.empty()) return "";
    std::error_code err;
    const fs::path save = fs::path(savePath);
    if (!fs::is_regular_file(save, err)) return "";
    const std::vector<char> data = ReadAll(save);
    if (data.empty()) return "";
    const fs::path dir = save.parent_path() / "backups";
    const std::string stem = save.stem().string();
    // The copies of this save, oldest first (their names sort by time).
    std::vector<fs::path> copies;
    if (fs::is_directory(dir, err))
        for (const auto& e : fs::directory_iterator(dir, err))
        {
            const std::string n = e.path().filename().string();
            if (e.is_regular_file(err) && n.rfind(stem + "-", 0) == 0 && e.path().extension() == ".sav") copies.push_back(e.path());
        }
    std::sort(copies.begin(), copies.end());
    if (!copies.empty() && ReadAll(copies.back()) == data) return "";
    fs::create_directories(dir, err);
    if (err) return "cannot create " + dir.string();
    char stamp[32];
    const time_t now = time(nullptr);
    strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", localtime(&now));
    fs::path copy = dir / (stem + "-" + stamp + ".sav");
    for (int i = 1; fs::exists(copy, err) && i < 100; i++) copy = dir / (stem + "-" + stamp + "-" + std::to_string(i) + ".sav");
    {
        std::ofstream out(copy, std::ios::binary);
        out.write(data.data(), (std::streamsize)data.size());
        if (!out) return "cannot write " + copy.string();
    }
    copies.push_back(copy);
    while ((int)copies.size() > kKeep)
    {
        fs::remove(copies.front(), err);
        copies.erase(copies.begin());
    }
    return copy.string();
}
}
