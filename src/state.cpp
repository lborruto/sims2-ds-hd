#include "state.h"

#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "Savestate.h"
#include "clock.h"

namespace sims2wide
{
bool SaveState(melonDS::NDS& nds, const std::string& path)
{
    melonDS::Savestate st;
    if (st.Error || !nds.DoSavestate(&st)) return false;
    st.Finish();
    // Written beside it first, checked, then put in its place: a full disk or a cut never leaves
    // a broken state where a good one was.
    const std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(st.Buffer(), 1, st.Length(), f) == st.Length();
    // On the disk before it takes the old one's place (a power cut after the rename never leaves
    // an empty file under the state's name).
    ok = fflush(f) == 0 && ok;
#ifdef _WIN32
    ok = _commit(_fileno(f)) == 0 && ok;
#else
    ok = fsync(fileno(f)) == 0 && ok;
#endif
    ok = (fclose(f) == 0) && ok;
    std::error_code ec;
    if (ok) std::filesystem::rename(tmp, path, ec);
    if (!ok || ec)
    {
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

bool LoadState(melonDS::NDS& nds, const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::vector<char> buf((std::istreambuf_iterator<char>(in)), {});
    melonDS::Savestate st(buf.data(), (melonDS::u32)buf.size(), false);
    if (st.Error || !nds.DoSavestate(&st)) return false;
    // The state carries the RTC as it was when it was saved: the game must see today's time,
    // never an older one (it calls a clock set back cheating).
    clock::Seed(nds);
    return true;
}

bool SaveStateTo(melonDS::NDS& nds, std::vector<u8>& out)
{
    melonDS::Savestate st;
    if (st.Error || !nds.DoSavestate(&st)) return false;
    st.Finish();
    out.assign((const u8*)st.Buffer(), (const u8*)st.Buffer() + st.Length());
    return true;
}

bool LoadStateFrom(melonDS::NDS& nds, std::vector<u8>& data)
{
    if (data.empty()) return false;
    melonDS::Savestate st(data.data(), (melonDS::u32)data.size(), false);
    if (st.Error || !nds.DoSavestate(&st)) return false;
    clock::Seed(nds);
    return true;
}
}
