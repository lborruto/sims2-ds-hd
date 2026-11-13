// The game's own icons for the HD menus: sprites decoded from the ROM's archive (romdata.h)
// as they are asked for and kept, at their own size and scaled up through the UI kit's
// scaler, so a menu redraw never decodes or scales twice. The archive stays open for the
// run (App::rom keeps it): which icons a menu needs (the pockets' items, say) is only known
// when it opens.
#pragma once
#include <map>
#include <tuple>
#include <vector>

#include "romdata.h"
#include "ui/kit.h"

namespace sims2hd
{
// A sprite in the archive, as the game's own 8-byte sprite definitions name one: the tiles,
// the descriptor and the palette resource. The game's definitions put the three side by side
// (tiles = descriptor - 1, palette = descriptor + 1) in every one seen: Of(descriptor).
struct SpriteRef
{
    int tiles = -1, desc = -1, pal = -1;
    static SpriteRef Of(int desc) { return {desc - 1, desc, desc + 1}; }
    bool operator<(const SpriteRef& o) const { return std::tie(tiles, desc, pal) < std::tie(o.tiles, o.desc, o.pal); }
};

// A sprite's frame in a look of its own (IconCache::Look): as it is, mirrored, grey (the colour
// taken out, lifted), dimmed (two fifths of its light).
enum class SpriteLook { Plain, Flip, Grey, Dim };

// The largest w or h IconCache::Scaled makes (a bad size is refused, not allocated).
constexpr int kMaxScaled = 4096;

class IconCache
{
public:
    // rom: the archive the icons are read from; must outlive the cache (null: no icon reads).
    explicit IconCache(const RomData* rom) : rom(rom) {}

    // How many frames the sprite has (0: it does not read).
    int Frames(SpriteRef s);
    // Frame `frame` of the sprite as decoded (straight alpha, colour 0 transparent); null when
    // it does not read (remembered too: a bad id is not decoded again). The pointer stays
    // valid for the cache's life.
    const SpriteFrame* Frame(SpriteRef s, int frame);
    // The same frame scaled to w x h through the kit's scaler (ImageItem's: nearest by the
    // whole factor, then bilinear; `scale` times its own size: nearest alone, every pixel a
    // scale x scale block); null as above, or for an empty size or one past kMaxScaled (4096) either way.
    const SpriteFrame* Scaled(SpriteRef s, int frame, int w, int h);
    const SpriteFrame* Scaled(SpriteRef s, int frame, int scale);
    // The image item that draws frame `frame` of s at `to` (`scaled`: Scaled's copy, the size `to`
    // has, drawn only when the frame does not read): the frame as it is, in both looks, so the kit
    // enlarges it at the window's own size (Pixel nearest by the whole factor, Smooth the pixel-art
    // scaler): never a pre-scaled copy blurred again at a fractional window scale.
    ui::UiItem Image(SpriteRef s, int frame, const SpriteFrame& scaled, Rect to);
    // Frame `frame` of s at tier `tier` (ui::kTierS, ui::kTierL: one per component), centred in
    // `box` (ui::TierImage of the frame as it is); an item that draws nothing when it does not read.
    ui::UiItem Tiered(SpriteRef s, int frame, int tier, Rect box);
    // Frame `frame` of s `scale` times its size (nearest: every pixel a block; 1-8) in `look`, its
    // colour times light / 255 (a fade: 0-255, the alpha kept); null when it does not read. The
    // copies at full light are kept for the cache's life, the others for the last two lights asked
    // (a fade asks a new one every frame): a pointer stays valid until two other lights are asked,
    // so the previous frame's composition never points at freed pixels while the next is built.
    const SpriteFrame* Look(SpriteRef s, int frame, SpriteLook look, int scale, int light = 255);

    const RomData* Rom() const { return rom; }

private:
    const RomData* rom;
    std::map<std::tuple<SpriteRef, int>, SpriteFrame> frames;            // w == 0: does not read
    std::map<std::tuple<SpriteRef, int, int, int>, SpriteFrame> scaled;  // (sprite, frame, w, h)
    std::map<std::tuple<SpriteRef, int, int, int>, SpriteFrame> looks;   // (sprite, frame, look, scale) at full light
    std::map<std::tuple<SpriteRef, int, int, int>, SpriteFrame> faded[2]; // the same at fadedLight[0] (now) and [1] (before)
    int fadedLight[2] = {-1, -1};
};

// (4c's screens) Frame `frame` of s at tier `tier` centred in `box` (IconCache::Tiered) appended to
// `items`, `opacity` its fill (ImageItem's: 0 opaque); nothing without icons.
void AddTiered(std::vector<ui::UiItem>& items, IconCache* icons, SpriteRef s, int frame, int tier, Rect box, u32 opacity = 0);
}
