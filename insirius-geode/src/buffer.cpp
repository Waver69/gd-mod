#include "buffer.hpp"
#include "config.hpp"
#include "state.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <algorithm>

using namespace geode::prelude;

namespace buffer {

int lastDelayTicks = 0, savedClicks = 0, forcedClicks = 0, heldForOrbs = 0;
bool pending = false;
std::string status;

namespace {
PlayerObject* g_dummy = nullptr;
CCDrawNode* g_drawNode = nullptr;
PlayLayer* g_layer = nullptr;
float g_dt = 1.f / 240.f;
bool g_sim = false;
bool g_simDead = false;

// ---- death-aware click state
struct Pending {
    bool active = false, p1 = true, released = false;
    int waited = 0, held = 0;
} g_pending;
int g_releaseIn[2] = {-1, -1};
bool g_waitUserRelease[2] = {false, false};

// ---- "keep early air clicks for orbs" state
struct Hold {
    bool gameHeld = false, waiting = false, consumed = false, releasePending = false;
    int ticks = 0;
} g_hold[2];

// ---- diagnostics: compare a prediction with reality now and then
struct Diag {
    int tick = 0, startAt = 120, runs = 0, t = 0;
    bool running = false;
    std::vector<CCPoint> pred, real;
} g_diag;

bool isFly(PlayerObject* p) { return p->m_isShip || p->m_isDart || p->m_isBird || p->m_isSwing; }

PlayerObject* playerFor(PlayLayer* pl, bool p1) {
    bool twoPlayer = pl->m_levelSettings && pl->m_levelSettings->m_twoPlayerMode;
    return (!p1 && twoPlayer && pl->m_player2) ? pl->m_player2 : pl->m_player1;
}

int ticksFromMs(int ms) { return std::max(1, ms * 240 / 1000); }

bool clickSurvives(PlayLayer* pl, PlayerObject* player, int hold) {
    auto& c = cfg();
    bool fly = isFly(player);
    int maxTicks = fly ? hold + c.pathFlyLookahead : c.pathLookahead;
    SimResult r = simulate(pl, player, true, hold, maxTicks, !fly, nullptr);
    if (!r.died) return true;
    if (fly) {
        // flying modes need constant input: only step in if the click is what kills you
        SimResult idle = simulate(pl, player, false, 0, maxTicks, false, nullptr);
        if (idle.died && r.deathTick >= idle.deathTick) return true;
    }
    return false;
}

void diagStep(PlayLayer* pl) {
    if (!cfg().bufferDiagnostics || !g_dummy || !pl->m_player1) return;
    g_diag.tick++;
    if (!g_diag.running && g_diag.tick >= g_diag.startAt && g_diag.runs < 8) {
        g_diag.pred.clear(); g_diag.real.clear();
        auto r = simulate(pl, pl->m_player1, false, 0, 60, false, &g_diag.pred);
        log::info("[buffer diag] run {}: real=({:.1f},{:.1f}) sim died={} at {} after {} ticks",
                  g_diag.runs, pl->m_player1->getPositionX(), pl->m_player1->getPositionY(), r.died, r.deathTick, r.ticks);
        g_diag.real.push_back(pl->m_player1->getPosition());
        g_diag.running = true; g_diag.t = 0;
    } else if (g_diag.running) {
        g_diag.real.push_back(pl->m_player1->getPosition());
        if (++g_diag.t >= 60) {
            float err = 0.f;
            for (size_t i = 0; i < g_diag.real.size() && i < g_diag.pred.size(); i++)
                err = std::max(err, g_diag.real[i].getDistance(g_diag.pred[i]));
            log::info("[buffer diag] run {}: largest difference sim vs real over 60 ticks = {:.2f} units", g_diag.runs, err);
            status = fmt::format("prediction check: off by {:.1f} units", err);
            g_diag.running = false;
            g_diag.startAt = g_diag.tick + 480;
            g_diag.runs++;
        }
    }
}
} // namespace

bool inSim() { return g_sim; }
bool isSimPlayer(PlayerObject* p) { return p && p == g_dummy; }
void markDeath() { g_simDead = true; }
void captureDt(PlayerObject* p, float dt) {
    if (!g_sim && g_layer && p == g_layer->m_player1 && dt > 0.f && dt < 0.1f) g_dt = dt;
}

void onLevelStart(PlayLayer* pl) {
    g_layer = pl;
    g_pending = Pending{};
    pending = false;
    for (auto& h : g_hold) h = Hold{};
    g_diag = Diag{};
    // hidden player used for simulation (same setup as Mega Overlay's "show trajectory")
    g_dummy = PlayerObject::create(1, 1, pl, pl, true);
    g_dummy->retain();
    g_dummy->setPosition({0, 105});
    g_dummy->setVisible(false);
    if (pl->m_objectLayer) pl->m_objectLayer->addChild(g_dummy);

    g_drawNode = CCDrawNode::create();
    g_drawNode->retain();
    if (pl->m_debugDrawNode && pl->m_debugDrawNode->getParent()) {
        pl->m_debugDrawNode->getParent()->addChild(g_drawNode);
        g_drawNode->setZOrder(pl->m_debugDrawNode->getZOrder() + 1);
    } else if (pl->m_objectLayer) {
        pl->m_objectLayer->addChild(g_drawNode, 1000);
    }
}

void onLevelEnd() {
    if (g_dummy) g_dummy->release();
    if (g_drawNode) g_drawNode->release();
    g_dummy = nullptr;
    g_drawNode = nullptr;
    g_layer = nullptr;
    g_pending = Pending{};
    pending = false;
}

SimResult simulate(PlayLayer* pl, PlayerObject* src, bool press, int hold, int maxTicks, bool stopOnLand,
                   std::vector<CCPoint>* path) {
    SimResult res;
    if (!g_dummy || !src || pl != g_layer) return res;
    auto* d = g_dummy;
    g_sim = true;
    g_simDead = false;

    d->spawnFromPlayer(src, false);
    d->setVisible(false);
    d->m_gravityMod = src->m_gravityMod;
    if (src->m_isOnGround) d->m_isOnGround = true;
    if (press) d->pushButton(PlayerButton::Jump);

    bool left = !d->m_isOnGround;
    if (path) path->push_back(d->getPosition());
    for (int t = 0; t < maxTicks; t++) {
        d->resetCollisionLog(false);
        pl->checkCollisions(d, g_dt, false);
        if (g_simDead) { res.died = true; res.deathTick = t; res.ticks = t + 1; break; }
        if (press && t == hold) d->releaseButton(PlayerButton::Jump);
        d->update(g_dt);
        d->updateSpecial(g_dt);
        d->updateRotation(g_dt);
        res.ticks = t + 1;
        if (path) path->push_back(d->getPosition());
        if (!d->m_isOnGround) left = true;
        else if (left && stopOnLand && t > hold) { res.landed = true; break; }
    }
    if (press && res.ticks <= hold) d->releaseButton(PlayerButton::Jump);
    g_sim = false;
    return res;
}

bool onUserButton(PlayLayer* pl, bool down, int button, bool p1) {
    auto& c = cfg();
    if (!c.bufferEnabled || !g_dummy || pl != g_layer || button != 1) return false;
    if ((p1 && !c.bufferP1) || (!p1 && !c.bufferP2)) return false;
    int pi = p1 ? 1 : 0;
    Hold& h = g_hold[pi];

    if (!down) {
        if (g_pending.active && g_pending.p1 == p1) { g_pending.released = true; return true; }
        if (g_waitUserRelease[pi]) { g_waitUserRelease[pi] = false; return false; }
        // early air click that hasn't been used yet: keep holding it (orbs)
        if (c.pathHoldAir && h.gameHeld && h.waiting && !h.consumed) { h.releasePending = true; return true; }
        h = Hold{};
        return false;
    }

    // new click
    if (g_pending.active && g_pending.p1 == p1) { g_pending.released = false; return true; }
    if (g_releaseIn[pi] >= 0) { gameButton(pl, false, 1, p1); g_releaseIn[pi] = -1; }
    if (h.gameHeld && h.releasePending) gameButton(pl, false, 1, p1);
    h = Hold{};

    PlayerObject* player = playerFor(pl, p1);
    if (!player || player->m_isDead) return false;
    bool fly = isFly(player);
    if ((fly && !c.pathFlyModes) || (!fly && !c.pathCubeModes)) return false;

    if (clickSurvives(pl, player, c.pathAssumeHold)) {
        // safe: let it through now. Remember it in case it was too early for an orb.
        h.gameHeld = true;
        h.waiting = !fly && !player->m_isOnGround;
        return false;
    }
    g_pending = Pending{};
    g_pending.active = true;
    g_pending.p1 = p1;
    pending = true;
    return true;
}

void onJump(PlayLayer* pl, PlayerObject* p) {
    if (g_sim || pl != g_layer) return;
    bool twoPlayer = pl->m_levelSettings && pl->m_levelSettings->m_twoPlayerMode;
    if (p == pl->m_player1 || !twoPlayer) g_hold[1].consumed = true;
    if (p == pl->m_player2 || !twoPlayer) g_hold[0].consumed = true;
}

void step(PlayLayer* pl) {
    auto& c = cfg();
    if (pl != g_layer) return;
    diagStep(pl);

    // scheduled releases after a delayed click
    for (int pi = 0; pi < 2; pi++)
        if (g_releaseIn[pi] >= 0 && --g_releaseIn[pi] < 0) gameButton(pl, false, 1, pi == 1);

    // early air clicks waiting for an orb / landing
    for (int pi = 0; pi < 2; pi++) {
        Hold& h = g_hold[pi];
        if (!h.waiting) continue;
        h.ticks++;
        bool done = h.consumed || h.ticks > ticksFromMs(c.holdWindowMs) || !c.bufferEnabled;
        if (done) {
            if (h.releasePending) {
                if (h.consumed) heldForOrbs++;
                gameButton(pl, false, 1, pi == 1);
                h = Hold{};
            } else {
                h.waiting = false;
            }
        }
    }

    if (!g_pending.active) return;
    Pending& p = g_pending;
    p.waited++;
    if (!p.released) p.held++;
    PlayerObject* player = playerFor(pl, p.p1);
    if (!player) { g_pending = Pending{}; pending = false; return; }

    int hold = p.released ? std::max(p.held, 1) : std::max(p.held, c.pathAssumeHold);
    bool safe = clickSurvives(pl, player, hold);
    bool timeout = p.waited >= ticksFromMs(c.pathMaxDelayMs);
    bool lastChance = false;
    if (!safe && !timeout) lastChance = simulate(pl, player, false, 0, 3, false, nullptr).died;
    if (!(safe || timeout || lastChance || !c.bufferEnabled)) return;

    int pi = p.p1 ? 1 : 0;
    gameButton(pl, true, 1, p.p1);
    lastDelayTicks = p.waited;
    if (safe) savedClicks++; else forcedClicks++;
    if (p.released) g_releaseIn[pi] = std::max(p.held, 1);
    else g_waitUserRelease[pi] = true;
    g_pending = Pending{};
    pending = false;
}

void onReset(PlayLayer* pl) {
    for (int pi = 0; pi < 2; pi++) {
        if (g_releaseIn[pi] >= 0 || g_waitUserRelease[pi] || g_hold[pi].releasePending)
            rawButton(pl, false, 1, pi == 1);
        g_releaseIn[pi] = -1;
        g_waitUserRelease[pi] = false;
        g_hold[pi] = Hold{};
    }
    g_pending = Pending{};
    pending = false;
    g_diag.running = false;
}

void drawPaths(PlayLayer* pl) {
    if (!g_drawNode || pl != g_layer) return;
    g_drawNode->clear();
    if (!cfg().showPath || !pl->m_player1 || pl->m_player1->m_isDead) return;
    std::vector<CCPoint> idle, click;
    simulate(pl, pl->m_player1, false, 0, cfg().pathPreviewTicks, false, &idle);
    simulate(pl, pl->m_player1, true, cfg().pathAssumeHold, cfg().pathPreviewTicks, false, &click);
    auto draw = [&](std::vector<CCPoint>& pts, ccColor4F col) {
        for (size_t i = 1; i < pts.size(); i++) g_drawNode->drawSegment(pts[i - 1], pts[i], 0.8f, col);
    };
    draw(idle, {1.f, 0.25f, 0.3f, 0.9f});
    draw(click, pending ? ccColor4F{0.75f, 0.45f, 1.f, 1.f} : ccColor4F{0.3f, 1.f, 0.45f, 0.9f});
}

} // namespace buffer

// ---------------------------------------------------------------- keep the simulation from touching the real level
using namespace buffer;

class $modify(SimGJBGL, GJBaseGameLayer) {
    void collisionCheckObjects(PlayerObject* player, gd::vector<GameObject*>* objects, int count, float dt) {
        if (!inSim()) return GJBaseGameLayer::collisionCheckObjects(player, objects, count, dt);
        // only blocks, spikes and slopes take part in the simulation
        gd::vector<GameObject*> backup = *objects;
        auto end = std::remove_if(objects->begin(), objects->end(), [](GameObject* o) {
            if (!o) return true;
            auto t = o->m_objectType;
            return t != GameObjectType::Solid && t != GameObjectType::Hazard &&
                   t != GameObjectType::AnimatedHazard && t != GameObjectType::Slope;
        });
        objects->erase(end, objects->end());
        GJBaseGameLayer::collisionCheckObjects(player, objects, static_cast<int>(objects->size()), dt);
        *objects = backup;
    }
    void flipGravity(PlayerObject* player, bool flip, bool noEffects) {
        if (inSim()) { player->flipGravity(flip, true); return; }
        GJBaseGameLayer::flipGravity(player, flip, noEffects);
    }
    void playerTouchedRing(PlayerObject* player, RingObject* ring) {
        if (inSim()) return;
        GJBaseGameLayer::playerTouchedRing(player, ring);
    }
    bool canBeActivatedByPlayer(PlayerObject* player, EffectGameObject* obj) {
        if (inSim()) return false;
        return GJBaseGameLayer::canBeActivatedByPlayer(player, obj);
    }
    void gameEventTriggered(GJGameEvent event, int material, int playerID) {
        if (inSim()) return;
        GJBaseGameLayer::gameEventTriggered(event, material, playerID);
    }
};

class $modify(SimPlayer, PlayerObject) {
    void update(float dt) {
        captureDt(this, dt);
        PlayerObject::update(dt);
    }
    void ringJump(RingObject* ring, bool skipCheck) {
        if (inSim()) return;
        PlayerObject::ringJump(ring, skipCheck);
    }
    void playSpiderDashEffect(CCPoint from, CCPoint to) {
        if (inSim()) return;
        PlayerObject::playSpiderDashEffect(from, to);
    }
};

class $modify(SimStreak, HardStreak) {
    void addPoint(CCPoint p) {
        if (inSim()) return;
        HardStreak::addPoint(p);
    }
};

class $modify(SimGameObject, GameObject) {
    void playShineEffect() {
        if (inSim()) return;
        GameObject::playShineEffect();
    }
};

class $modify(SimEffectObject, EffectGameObject) {
    void triggerObject(GJBaseGameLayer* layer, int uniqueID, gd::vector<int> const* remap) {
        if (inSim()) return;
        EffectGameObject::triggerObject(layer, uniqueID, remap);
    }
};

class $modify(SimPlayLayer, PlayLayer) {
    void playEndAnimationToPos(CCPoint pos) {
        if (inSim()) return;
        PlayLayer::playEndAnimationToPos(pos);
    }
    void playPlatformerEndAnimationToPos(CCPoint pos, bool instant) {
        if (inSim()) return;
        PlayLayer::playPlatformerEndAnimationToPos(pos, instant);
    }
};
