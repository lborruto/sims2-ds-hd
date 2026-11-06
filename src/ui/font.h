// The UI's HD text: Nunito (assets/fonts, SIL OFL 1.1, embedded into the program at build time)
// through stb_truetype. Sizes are pixels per em; text is UTF-8 (a bad byte reads as U+FFFD, a
// letter the font lacks is drawn as '?'). Glyphs are rendered once per size, weight and quarter
// pixel of horizontal phase and kept: same text, same pixels.
#pragma once
#include <string>
#include <vector>

#include "common.h"

namespace sims2wide::ui
{
enum class Weight { Bold, ExtraBold };

// The code points (>= U+0020) the font was asked for and lacks, drawn or measured as '?', since the
// last reset, ascending: what the checks (--measure-string, --measure-menu) fail on.
std::vector<u32> MissingGlyphs();
void ResetMissingGlyphs();

// Ascent and descent (below the baseline, positive) and the line's height, for sizePx.
struct LineMetrics { float ascent, descent, lineHeight; };
LineMetrics Metrics(float sizePx, Weight w = Weight::Bold);

// The text's advance at sizePx, kerning included (one line: '\n' counts as nothing).
float TextWidth(const std::string& utf8, float sizePx, Weight w = Weight::Bold);

// Word wrap to widthPx: lines split at spaces (runs of spaces fold into one) and at '\n'; a word
// wider than a line is broken between letters (at least one letter a line). Empty text: no line.
std::vector<std::string> Wrap(const std::string& utf8, float sizePx, float widthPx, Weight w = Weight::Bold);

// One line's coverage added into an 8-bit mask of w x h (clipped to it): the pen starts at x and
// the baseline at y, both in mask pixels, sizePx the size in mask pixels.
void DrawLine(const std::string& utf8, float sizePx, Weight w, float x, float y, u8* mask, int mw, int mh);
}
