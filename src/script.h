// Input scripts for headless runs: one event per line, "<frame> <frames held> <what>", what is a
// key name (several joined by '+'), "blow" (the microphone's blow key) or "touch <x> <y>".
// '#' starts a comment.
#pragma once
#include <string>
#include <vector>

#include "common.h"

namespace sims2wide
{
struct InputEvent { int frame, len; u32 keys; bool blow; bool touch; int tx, ty; };

// Exits with a message on an unreadable file or an unknown key name.
std::vector<InputEvent> LoadScript(const std::string& path);

// The keys, blow and touch the script holds at this frame (keys OR-ed in, the others set when
// held).
void ApplyScript(const std::vector<InputEvent>& script, long frame, u32& keys, bool& blow, bool& touching, int& tx, int& ty);
}
