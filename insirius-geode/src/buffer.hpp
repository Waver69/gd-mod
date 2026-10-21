#pragma once
#include <vector>
#include <Geode/Geode.hpp>

// Path predictor + death-aware input buffer.
//
// A hidden copy of the player is simulated with GD's own physics
// (PlayerObject::update + GJBaseGameLayer::checkCollisions, the same approach
// "show trajectory" hacks use). When you click, "click now" is simulated; if it
// dies, the click is held back and re-checked every physics tick until a
// click survives, then sent with your real hold length.
namespace buffer {

struct SimResult {
    bool died = false;
    int  deathTick = -1;
    bool landed = false;
    int  ticks = 0;
};

void onLevelStart(PlayLayer* pl);
void onLevelEnd();
bool inSim();
bool isSimPlayer(PlayerObject* p);
void markDeath();
void captureDt(PlayerObject* p, float dt);

SimResult simulate(PlayLayer* pl, PlayerObject* src, bool press, int hold, int maxTicks, bool stopOnLand,
                   std::vector<cocos2d::CCPoint>* path);

// buffer
bool onUserButton(PlayLayer* pl, bool down, int button, bool p1);  // true = taken over
void step(PlayLayer* pl);                                           // every physics tick
void onReset(PlayLayer* pl);
void onJump(PlayLayer* pl, PlayerObject* p);                        // PlayerObject::incrementJumps
void drawPaths(PlayLayer* pl);

extern int lastDelayTicks, savedClicks, forcedClicks, heldForOrbs;
extern bool pending;
extern std::string status;
}
