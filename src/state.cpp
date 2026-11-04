#include "state.h"

#include <cstdio>
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
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fwrite(st.Buffer(), 1, st.Length(), f);
    fclose(f);
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
}
