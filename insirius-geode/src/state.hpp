#pragma once
#include <deque>
#include <chrono>
#include <cstdint>

class PlayLayer;
class GJBaseGameLayer;

struct State {
    int    attempts = 1;
    int    totalClicks = 0;
    std::deque<double> clickTimes;   // seconds, for CPS
    double sessionStart = 0;

    int  frames = 0, deathFrames = 0, noclipDeaths = 0;
    bool diedThisFrame = false, diedLastFrame = false;
    float flash = 0.f;

    int  stepsQueued = 0;
    int  acCounter = 0;
    bool acDown = false;
    float rgbHue = 0.f;

    bool menuOpen = false;

    void resetRun() {
        frames = deathFrames = noclipDeaths = 0;
        diedThisFrame = diedLastFrame = false;
        flash = 0.f;
    }
};

State& st();
double nowSeconds();
uint32_t frameOf(GJBaseGameLayer* layer);
bool safeModeOn();

// the one function every input that reaches the game goes through
void gameButton(GJBaseGameLayer* layer, bool down, int button, bool p1);
// calls the original (unhooked) handleButton
void rawButton(GJBaseGameLayer* layer, bool down, int button, bool p1);
