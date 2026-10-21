#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Input macro / replay bot.
// Frames are physics ticks (240/s) taken from GJBaseGameLayer::m_levelTime,
// so practice-mode checkpoints rewind the macro automatically.
// Inputs are recorded when they reach the game and replayed at the start of
// the matching physics step (GJBaseGameLayer::processCommands).
struct MacroAction {
    uint32_t frame;
    bool down;
    uint8_t button;
    bool p1;
};

class Macro {
public:
    enum Mode { Off, Record, Play };
    Mode mode = Off;
    std::vector<MacroAction> actions;
    std::string name = "macro";
    size_t index = 0;
    bool ignoreUserInput = true;
    bool autoStopAtEnd = true;
    std::string status;

    void startRecord();
    void startPlay();
    void stop();
    void clear();

    // hooks
    void onInput(uint32_t frame, bool down, int button, bool p1);  // a button reached the game
    template <typename F> void onStep(uint32_t frame, F&& press) {
        if (mode != Play) return;
        while (index < actions.size() && actions[index].frame <= frame) {
            auto& a = actions[index++];
            press(a.down, a.button, a.p1);
        }
    }
    // after a reset / checkpoint respawn. `release(button, p1)` sends a release.
    template <typename F> void onReset(uint32_t frame, F&& release) {
        if (mode == Record) {
            while (!actions.empty() && actions.back().frame >= frame) actions.pop_back();
            // release anything left held at the cut
            bool held[4][2] = {};
            for (auto& a : actions) if (a.button < 4) held[a.button][a.p1 ? 1 : 0] = a.down;
            for (int b = 0; b < 4; b++) for (int p = 0; p < 2; p++)
                if (held[b][p]) { actions.push_back({frame, false, uint8_t(b), p == 1}); release(b, p == 1); }
        } else if (mode == Play) {
            index = 0;
            while (index < actions.size() && actions[index].frame < frame) index++;
        }
    }

    bool save();
    bool load(const std::string& name);
    std::vector<std::string> list();
    static std::string dir();
};

Macro& macro();
