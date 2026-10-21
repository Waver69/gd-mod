// Game hooks for insirius's mod menu (Geode version - Windows + Android).
#include "config.hpp"
#include "state.hpp"
#include "buffer.hpp"
#include "macro.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/GameStatsManager.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include <Geode/modify/LevelAreaInnerLayer.hpp>
#include <Geode/modify/LevelTools.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/CCParticleSystemQuad.hpp>
#include <Geode/modify/CCTransitionFade.hpp>
#include <Geode/modify/CCTextInputNode.hpp>
#include <Geode/modify/SliderTouchLogic.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/OptionsLayer.hpp>
#include <Geode/modify/SecretLayer2.hpp>
#include <Geode/modify/CreatorLayer.hpp>
#include <cmath>

using namespace geode::prelude;

State& st() {
    static State s;
    return s;
}

double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

uint32_t frameOf(GJBaseGameLayer* layer) {
    return static_cast<uint32_t>(std::llround(layer->m_gameState.m_levelTime * 240.0));
}

bool safeModeOn() {
    auto& c = cfg();
    return c.safeMode || (c.autoSafeMode && cheatsActive());
}

namespace {
thread_local bool g_raw = false;   // calls that must skip our own handleButton logic
std::vector<GameObject*> g_coins;
bool g_coinsPending = false;

ccColor3B hsv(float h, float s, float v) {
    h = std::fmod(h, 1.f) * 6.f;
    int i = static_cast<int>(h);
    float f = h - i, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    float r, g, b;
    switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return {GLubyte(r * 255), GLubyte(g * 255), GLubyte(b * 255)};
}
} // namespace

void rawButton(GJBaseGameLayer* layer, bool down, int button, bool p1) {
    g_raw = true;
    layer->handleButton(down, button, p1);
    g_raw = false;
}

void gameButton(GJBaseGameLayer* layer, bool down, int button, bool p1) {
    if (PlayLayer::get() == layer) macro().onInput(frameOf(layer), down, button, p1);
    rawButton(layer, down, button, p1);
}

// ---------------------------------------------------------------- global
class $modify(InsScheduler, CCScheduler) {
    void update(float dt) {
        auto& c = cfg();
        if (c.speedhack && c.speed > 0.f) dt *= c.speed;
        CCScheduler::update(dt);
    }
};

class $modify(InsFMOD, FMODAudioEngine) {
    void update(float dt) {
        FMODAudioEngine::update(dt);
        auto& c = cfg();
        static float last = 1.f;
        float pitch = (c.speedhack && c.speedAudio && c.speed > 0.f) ? c.speed : 1.f;
        if (pitch == last || !m_system) return;
        FMOD::ChannelGroup* master = nullptr;
        if (m_system->getMasterChannelGroup(&master) == FMOD_OK && master) master->setPitch(pitch);
        last = pitch;
    }
};

class $modify(InsParticles, CCParticleSystemQuad) {
    void draw() {
        if (cfg().noParticles) return;
        CCParticleSystemQuad::draw();
    }
};

class $modify(InsFade, CCTransitionFade) {
    static CCTransitionFade* create(float t, CCScene* scene) {
        if (cfg().noTransition) t = 0.f;
        return CCTransitionFade::create(t, scene);
    }
};

// ---------------------------------------------------------------- gameplay
class $modify(InsGJBGL, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool p1) {
        auto pl = PlayLayer::get();
        if (g_raw || pl != static_cast<GJBaseGameLayer*>(this)) return GJBaseGameLayer::handleButton(down, button, p1);
        auto& s = st();
        if (macro().mode == Macro::Play && cfg().macroIgnoreInput) return;
        if (down) {
            s.totalClicks++;
            s.clickTimes.push_back(nowSeconds());
        }
        if (buffer::onUserButton(pl, down, button, p1)) return;
        gameButton(this, down, button, p1);
    }

    void processCommands(float dt, bool half, bool last) {
        if (auto pl = PlayLayer::get(); pl == static_cast<GJBaseGameLayer*>(this) && !buffer::inSim()) {
            buffer::step(pl);
            macro().onStep(frameOf(this), [this](bool d, int b, bool p) { rawButton(this, d, b, p); });
        }
        GJBaseGameLayer::processCommands(dt, half, last);
    }

    void update(float dt) {
        auto& c = cfg();
        auto& s = st();
        auto pl = PlayLayer::get();
        if (pl != static_cast<GJBaseGameLayer*>(this)) return GJBaseGameLayer::update(dt);

        if (c.frameStepper) {
            if (s.stepsQueued <= 0) return;
            s.stepsQueued--;
            dt = 1.f / 240.f;
        }
        if (c.jumpHack) {
            if (m_player1) m_player1->m_isOnGround = true;
            if (m_player2) m_player2->m_isOnGround = true;
        }
        if (c.autoclicker) {
            if (++s.acCounter >= (s.acDown ? c.acHold : c.acGap)) {
                s.acCounter = 0;
                s.acDown = !s.acDown;
                if (c.acP1) gameButton(this, s.acDown, 1, true);
                if (c.acP2) gameButton(this, s.acDown, 1, false);
            }
        } else if (s.acDown) {
            s.acDown = false;
            if (c.acP1) gameButton(this, false, 1, true);
            if (c.acP2) gameButton(this, false, 1, false);
        }
        if (g_coinsPending && m_player1) {
            g_coinsPending = false;
            if (c.autoPickupCoins && !pl->m_isPracticeMode) {
                for (auto* coin : g_coins) {
                    if (!coin) continue;
                    destroyObject(coin);
                    pickupItem(static_cast<EffectGameObject*>(coin));
                }
            }
        }

        s.frames++;
        if (s.diedThisFrame) {
            s.deathFrames++;
            if (!s.diedLastFrame) s.noclipDeaths++;
        }
        s.diedLastFrame = s.diedThisFrame;
        s.diedThisFrame = false;

        GJBaseGameLayer::update(dt);
        buffer::drawPaths(pl);

        if (c.rgbIcons) {
            s.rgbHue += dt * c.rgbSpeed;
            auto a = hsv(s.rgbHue, 0.8f, 1.f), b = hsv(s.rgbHue + 0.5f, 0.8f, 1.f);
            for (auto* p : {m_player1, m_player2}) if (p) { p->setColor(a); p->setSecondColor(b); }
        }
    }

    void shakeCamera(float a, float b, float c) {
        if (cfg().noShake || buffer::inSim()) return;
        GJBaseGameLayer::shakeCamera(a, b, c);
    }

    void toggleFlipped(bool flip, bool noEffects) {
        if (cfg().noMirror) return;
        GJBaseGameLayer::toggleFlipped(flip, noEffects);
    }
};

class $modify(InsPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        g_coins.clear();
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        auto& s = st();
        s.attempts = 1;
        s.resetRun();
        s.sessionStart = nowSeconds();
        g_coinsPending = true;
        buffer::onLevelStart(this);
        if (cfg().autoPractice && !m_isPracticeMode) togglePracticeMode(true);
        return true;
    }

    void onQuit() {
        buffer::onLevelEnd();
        if (macro().mode == Macro::Record) macro().stop();
        g_coins.clear();
        PlayLayer::onQuit();
    }

    void addObject(GameObject* obj) {
        PlayLayer::addObject(obj);
        if (obj && (obj->m_objectType == GameObjectType::SecretCoin || obj->m_objectType == GameObjectType::UserCoin))
            g_coins.push_back(obj);
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        auto& c = cfg();
        auto& s = st();
        // path predictor simulation: just remember the simulated player died
        if (buffer::inSim()) {
            if (buffer::isSimPlayer(player) && obj != m_anticheatSpike) buffer::markDeath();
            return;
        }
        // always let the anticheat spike through
        if (m_anticheatSpike && obj == m_anticheatSpike) return PlayLayer::destroyPlayer(player, obj);

        bool isP1 = !player || player == m_player1;
        if (c.noclip && ((isP1 && c.noclipP1) || (!isP1 && c.noclipP2))) {
            s.diedThisFrame = true;
            if (c.noclipFlash) s.flash = 0.35f;
            return;
        }
        bool old = m_isTestMode;
        if (safeModeOn()) m_isTestMode = true;
        PlayLayer::destroyPlayer(player, obj);
        m_isTestMode = old;
    }

    void resetLevel() {
        auto& s = st();
        s.attempts++;
        s.resetRun();
        PlayLayer::resetLevel();
        buffer::onReset(this);
        macro().onReset(frameOf(this), [this](int b, bool p) { rawButton(this, false, b, p); });
        g_coinsPending = true;
    }

    void levelComplete() {
        bool old = m_isTestMode;
        if (safeModeOn()) m_isTestMode = true;
        PlayLayer::levelComplete();
        m_isTestMode = old;
        if (macro().mode == Macro::Record) macro().stop();
    }

    void storeCheckpoint(CheckpointObject* obj) {
        if (!cfg().checkpointLimit) return PlayLayer::storeCheckpoint(obj);
        m_checkpointArray->addObject(obj);
        PlayLayer::addToSection(obj->m_physicalCheckpointObject);
    }

    void updateProgressbar() {
        PlayLayer::updateProgressbar();
        auto& c = cfg();
        if (!c.showHitboxes && !c.hitboxesOnDeath) return;
        bool dead = m_player1 && m_player1->m_isDead;
        if (c.showHitboxes || (c.hitboxesOnDeath && dead)) {
            if (!(m_isPracticeMode && GameManager::get()->getGameVariable("0166"))) PlayLayer::updateDebugDraw();
            if (m_debugDrawNode) m_debugDrawNode->setVisible(true);
        }
    }
};

class $modify(InsPlayer, PlayerObject) {
    void playDeathEffect() {
        if (cfg().noDeathEffect) return;
        PlayerObject::playDeathEffect();
    }
    void playSpawnEffect() {
        if (cfg().noSpawnEffect) return;
        PlayerObject::playSpawnEffect();
    }
    void incrementJumps() {
        if (buffer::inSim()) return;
        PlayerObject::incrementJumps();
        if (auto pl = PlayLayer::get()) buffer::onJump(pl, this);
    }
};

class $modify(InsPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        if (cfg().hidePause) setVisible(false);
    }
};

// ---------------------------------------------------------------- bypasses
class $modify(InsGM, GameManager) {
    bool isIconUnlocked(int id, IconType type) {
        if (GameManager::isIconUnlocked(id, type)) return true;
        return cfg().unlockIcons;
    }
    bool isColorUnlocked(int id, UnlockType type) {
        if (GameManager::isColorUnlocked(id, type)) return true;
        return cfg().unlockIcons;
    }
    bool getUGV(char const* key) {
        if (GameManager::getUGV(key)) return true;
        auto& c = cfg();
        if (!key) return false;
        if (c.treasureRoom && !std::strcmp(key, "5")) return true;
        if (c.unlockShops && (!std::strcmp(key, "11") || !std::strcmp(key, "20") || !std::strcmp(key, "34") || !std::strcmp(key, "35"))) return true;
        if (c.unlockVaults && !std::strcmp(key, "4")) return true;
        return false;
    }
    bool getGameVariable(char const* key) {
        if (cfg().accuratePercent && key && !std::strcmp(key, "0126")) return true;
        return GameManager::getGameVariable(key);
    }
};

namespace { int g_fakeStat = -1; bool g_towerInit = false; }

class $modify(InsGSM, GameStatsManager) {
    bool isItemUnlocked(UnlockType type, int id) {
        if (GameStatsManager::isItemUnlocked(type, id)) return true;
        auto& c = cfg();
        if (type == UnlockType::GJItem) {
            if (c.unlockItems && (id == 16 || id == 17)) return true;
            if (c.unlockVaults && id >= 1 && id <= 4) return true;
        }
        return false;
    }
    int getStat(char const* key) {
        int real = GameStatsManager::getStat(key);
        if (g_fakeStat >= 0 && real < g_fakeStat) return g_fakeStat;
        return real;
    }
    bool hasCompletedMainLevel(int id) {
        if (g_towerInit && id >= 5001 && id <= 5004) return true;
        return GameStatsManager::hasCompletedMainLevel(id);
    }
};

namespace {
struct FakeStat {
    int old;
    explicit FakeStat(int v) : old(g_fakeStat) { if (cfg().unlockVaults) g_fakeStat = v; }
    ~FakeStat() { g_fakeStat = old; }
};
}

class $modify(InsOptions, OptionsLayer) {
    void customSetup() { FakeStat f(10); OptionsLayer::customSetup(); }
    void onSecretVault(CCObject* s) { FakeStat f(10); OptionsLayer::onSecretVault(s); }
};
class $modify(InsSecret2, SecretLayer2) {
    bool init() { FakeStat f(200); return SecretLayer2::init(); }
    void onSecretLevel(CCObject* s) { FakeStat f(200); SecretLayer2::onSecretLevel(s); }
};
class $modify(InsCreator, CreatorLayer) {
    bool init() { FakeStat f(50); return CreatorLayer::init(); }
    void onSecretVault(CCObject* s) { FakeStat f(51); CreatorLayer::onSecretVault(s); }
};

class $modify(InsGLM, GameLevelManager) {
    GJGameLevel* getMainLevel(int id, bool noString) {
        auto lvl = GameLevelManager::getMainLevel(id, noString);
        if (lvl && cfg().mainLevels && lvl->m_requiredCoins > 0) lvl->m_requiredCoins = 0;
        return lvl;
    }
};

class $modify(InsTower, LevelAreaInnerLayer) {
    bool init(bool returning) {
        g_towerInit = cfg().mainLevels;
        bool r = LevelAreaInnerLayer::init(returning);
        g_towerInit = false;
        return r;
    }
};

class $modify(InsLevelTools, LevelTools) {
    static bool verifyLevelIntegrity(gd::string str, int id) {
        bool r = LevelTools::verifyLevelIntegrity(str, id);
        return cfg().verifyBypass ? true : r;
    }
};

class $modify(InsTextInput, CCTextInputNode) {
    void updateLabel(gd::string text) {
        if (cfg().textLength) setMaxLabelLength(9999);
        if (cfg().charFilter)
            setAllowedChars("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~ ");
        CCTextInputNode::updateLabel(text);
    }
};

class $modify(InsSlider, SliderTouchLogic) {
    void ccTouchMoved(CCTouch* touch, CCEvent* event) {
        if (!cfg().sliderLimit) return SliderTouchLogic::ccTouchMoved(touch, event);
        if (m_thumb->m_vertical) m_thumb->setPositionY(convertToNodeSpace(touch->getLocation()).y);
        else m_thumb->setPositionX(convertToNodeSpace(touch->getLocation()).x);
        m_slider->updateBar();
        m_thumb->activate();
    }
};

class $modify(InsEditorUI, EditorUI) {
    void onNewCustomItem(CCObject* sender) {
        if (!cfg().customObjBypass) return EditorUI::onNewCustomItem(sender);
        CCArray* objects;
        if (m_selectedObjects && m_selectedObjects->count()) objects = m_selectedObjects;
        else {
            objects = CCArray::create();
            if (m_selectedObject) objects->addObject(m_selectedObject);
        }
        if (!objects->count()) return;
        GameManager::get()->addNewCustomObject(copyObjects(objects, false, false));
        m_selectedObjectIndex = 0;
        reloadCustomItems();
    }
};
