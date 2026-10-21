#include "config.hpp"
#include "macro.hpp"
#include <Geode/Geode.hpp>
#include <functional>
#include <vector>

using namespace geode::prelude;

Config& cfg() {
    static Config c;
    return c;
}

bool cheatsActive() {
    auto& c = cfg();
    return c.noclip || c.jumpHack || (c.speedhack && c.speed != 1.0f) || c.autoclicker || c.frameStepper ||
           c.bufferEnabled || c.noMirror || c.autoPickupCoins || macro().mode == Macro::Play;
}

namespace {
struct Entry {
    const char* name;
    std::function<void()> load;
    std::function<void()> save;
};

template <typename T> Entry entry(const char* name, T& ref) {
    return {name,
            [name, &ref] { ref = Mod::get()->getSavedValue<T>(name, ref); },
            [name, &ref] { Mod::get()->setSavedValue<T>(name, ref); }};
}

std::vector<Entry>& entries() {
    auto& c = cfg();
    static std::vector<Entry> list = {
#define E(x) entry(#x, c.x)
        E(uiScale), entry("accent0", c.accent[0]), entry("accent1", c.accent[1]), entry("accent2", c.accent[2]),
        E(showWatermark), E(buttonX), E(buttonY),
        E(noclip), E(noclipP1), E(noclipP2), E(noclipFlash), E(jumpHack), E(showHitboxes), E(hitboxesOnDeath),
        E(noDeathEffect), E(noSpawnEffect), E(noMirror), E(noShake), E(rgbIcons), E(rgbSpeed), E(showPath),
        E(pathPreviewTicks),
        E(speedhack), E(speed), E(speedAudio), E(fpsUnlock), E(fps), E(noParticles), E(noTransition), E(hidePause),
        E(unlockIcons), E(unlockItems), E(unlockShops), E(unlockVaults), E(treasureRoom), E(mainLevels),
        E(customObjBypass), E(sliderLimit), E(charFilter), E(textLength), E(checkpointLimit), E(verifyBypass),
        E(safeMode), E(autoSafeMode),
        E(accuratePercent), E(autoPickupCoins), E(autoPractice),
        E(bufferEnabled), E(pathMaxDelayMs), E(pathLookahead), E(pathFlyLookahead), E(pathAssumeHold),
        E(pathCubeModes), E(pathFlyModes), E(pathHoldAir), E(holdWindowMs), E(bufferP1), E(bufferP2),
        E(bufferDiagnostics),
        E(autoclicker), E(acHold), E(acGap), E(acP1), E(acP2), E(frameStepper), E(macroIgnoreInput), E(macroName),
        E(labels), E(labelCorner), E(labelScale), E(labelAlpha), E(lblCheat), E(lblFps), E(lblCps), E(lblClicks),
        E(lblPercent), E(lblAttempts), E(lblNoclipAcc), E(lblNoclipDeaths), E(lblSession), E(lblClock),
        E(lblBuffer), E(lblCustom), E(customText), E(menuKey), E(stepKey),
#undef E
    };
    return list;
}
} // namespace

void Config::load() {
    for (auto& e : entries()) e.load();
    auto b = Mod::get()->getSavedValue<matjson::Value>("binds", matjson::Value::object());
    binds.clear();
    if (b.isObject()) {
        for (auto& [k, v] : b) {
            if (auto n = v.asInt()) binds[k] = static_cast<int>(n.unwrap());
        }
    }
}

void Config::save() {
    for (auto& e : entries()) e.save();
    auto b = matjson::Value::object();
    for (auto& [k, v] : binds) if (v) b[k] = v;
    Mod::get()->setSavedValue("binds", b);
}
