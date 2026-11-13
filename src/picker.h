// Choosing the ROM when started without one, and telling the player about errors.
#pragma once
#include <string>

namespace sims2hd
{
// On the terminal always, and in a dialog box too when dialog is set (started by a double click).
void Tell(bool dialog, const std::string& msg);
// The system's file picker, in the folder of the ROM chosen last time; "" when cancelled.
std::string PickRom();
}
