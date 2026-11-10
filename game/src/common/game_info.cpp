#include "app/app.h"
#include "core/input.h"
#include "gpu/backend.h"

namespace sheroes {
void ScriptPad(int port, input::Pad& pad);  // pad_script.cpp
}

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"Sonic Heroes",              // window and splash
    .short_title = L"Sonic Heroes",        // message boxes
    .name = L"SHEROES",                 // window classes, crash dumps, handover event
    .log = "sheroes",                   // log file prefix
    .settings = "sheroes-settings.toml",  // settings file beside the exe
    .splash = L"splash",                  // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe with its folders beside it.",
    .images = {{"default.xbe", "SHEROES"}},  // each recompiled xbe and its exe, for title launches
    .options = {},                        // game settings kept in the settings file
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = nullptr,                      // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = nullptr,                   // window messages first; true takes one
    .ui = nullptr,                        // ImGui drawing each frame
    .pad = sheroes::ScriptPad,          // scripted test presses (pad_script.cpp), none unless asked for
    .rumble = nullptr,                    // changes a rumble before it is set
};
