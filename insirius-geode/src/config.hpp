#pragma once
#include <string>
#include <map>

// All settings. Saved with Geode's per-mod saved values (works on PC and Android).
struct Config {
    // menu
    float uiScale = 1.0f;
    float accent[3] = {0.60f, 0.35f, 1.00f};
    bool  showWatermark = true;
    float buttonX = 30.f, buttonY = 120.f;   // mobile open button position

    // player
    bool noclip = false, noclipP1 = true, noclipP2 = true, noclipFlash = true;
    bool jumpHack = false;
    bool showHitboxes = false, hitboxesOnDeath = false;
    bool noDeathEffect = false, noSpawnEffect = false;
    bool noMirror = false, noShake = false;
    bool rgbIcons = false;
    float rgbSpeed = 0.5f;
    bool showPath = false;
    int  pathPreviewTicks = 120;

    // global
    bool  speedhack = false;
    float speed = 1.0f;
    bool  speedAudio = true;
    bool  fpsUnlock = false;
    float fps = 240.0f;
    bool  noParticles = false, noTransition = false, hidePause = false;

    // bypass
    bool unlockIcons = false, unlockItems = false, unlockShops = false, unlockVaults = false;
    bool treasureRoom = false, mainLevels = false, customObjBypass = false;
    bool sliderLimit = false, charFilter = false, textLength = false, checkpointLimit = false;
    bool verifyBypass = false, safeMode = false, autoSafeMode = true;

    // level
    bool accuratePercent = false, autoPickupCoins = false, autoPractice = false;

    // input buffer (path predictor)
    bool bufferEnabled = false;
    int  pathMaxDelayMs = 150;
    int  pathLookahead = 160;
    int  pathFlyLookahead = 24;
    int  pathAssumeHold = 6;
    bool pathCubeModes = true, pathFlyModes = true, pathHoldAir = true;
    int  holdWindowMs = 150;
    bool bufferP1 = true, bufferP2 = true;
    bool bufferDiagnostics = true;

    // tools
    bool autoclicker = false;
    int  acHold = 2, acGap = 2;
    bool acP1 = true, acP2 = false;
    bool frameStepper = false;
    bool macroIgnoreInput = true;
    std::string macroName = "macro";

    // labels
    bool labels = true;
    int  labelCorner = 0;
    float labelScale = 1.0f, labelAlpha = 0.85f;
    bool lblCheat = true, lblFps = true, lblCps = true, lblClicks = false, lblPercent = true;
    bool lblAttempts = true, lblNoclipAcc = true, lblNoclipDeaths = true, lblSession = false;
    bool lblClock = false, lblBuffer = true, lblCustom = false;
    std::string customText = "insirius on top";

    // PC keybinds: setting key -> cocos key code
    int menuKey = 9;                         // KEY_Tab
    int stepKey = 67;                        // KEY_C
    std::map<std::string, int> binds;

    void load();
    void save();
};

Config& cfg();
bool cheatsActive();
