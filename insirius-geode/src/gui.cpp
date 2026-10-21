// ImGui menu + labels (mouse on PC, touch on Android).
#include "config.hpp"
#include "state.hpp"
#include "buffer.hpp"
#include "macro.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <imgui-cocos.hpp>
#include <ctime>
#include <vector>
#include <string>

using namespace geode::prelude;

namespace {
constexpr const char* kTitle = "insirius's mod menu";
constexpr const char* kVersion = "v2.0";

std::string g_capture;
bool g_dirty = false;
double g_lastSave = 0;
int g_resetLayout = 0;
double g_toastUntil = 0;

enum Win { W_PLAYER, W_GLOBAL, W_BYPASS, W_LEVEL, W_COSMETIC };
struct Hack { const char* key; const char* label; bool* value; const char* tip; int window; };

std::vector<Hack>& hacks() {
    auto& c = cfg();
    static std::vector<Hack> list = {
        {"noclip", "Noclip", &c.noclip, "You can't die. GD's anticheat spike is still let through.", W_PLAYER},
        {"jumpHack", "Jump Hack", &c.jumpHack, "Jump in mid-air.", W_PLAYER},
        {"showHitboxes", "Show Hitboxes", &c.showHitboxes, "Hitboxes outside practice mode too.", W_PLAYER},
        {"hitboxesOnDeath", "Hitboxes on Death", &c.hitboxesOnDeath, "Show hitboxes when you die.", W_PLAYER},
        {"noDeathEffect", "No Death Effect", &c.noDeathEffect, "", W_PLAYER},
        {"noSpawnEffect", "No Respawn Flash", &c.noSpawnEffect, "", W_PLAYER},
        {"noMirror", "No Mirror Portals", &c.noMirror, "", W_PLAYER},
        {"noShake", "No Camera Shake", &c.noShake, "", W_PLAYER},
        {"showPath", "Show Path Prediction", &c.showPath, "Red = if you don't click, green = if you click now, purple = a click is being held back.", W_PLAYER},

        {"speedhack", "Speedhack", &c.speedhack, "", W_GLOBAL},
        {"speedAudio", "Speedhack Audio", &c.speedAudio, "", W_GLOBAL},
        {"fpsUnlock", "FPS Unlock", &c.fpsUnlock, "", W_GLOBAL},
        {"noParticles", "No Particles", &c.noParticles, "", W_GLOBAL},
        {"noTransition", "Instant Transitions", &c.noTransition, "", W_GLOBAL},
        {"hidePause", "Hide Pause Menu", &c.hidePause, "", W_GLOBAL},

        {"safeMode", "Safe Mode", &c.safeMode, "Never save progress / completions.", W_BYPASS},
        {"autoSafeMode", "Auto Safe Mode", &c.autoSafeMode, "Safe mode turns on by itself while any cheat is on.", W_BYPASS},
        {"unlockIcons", "Unlock All Icons", &c.unlockIcons, "", W_BYPASS},
        {"unlockItems", "Unlock Items", &c.unlockItems, "Practice music sync + music unlocker.", W_BYPASS},
        {"unlockShops", "Unlock Shops", &c.unlockShops, "", W_BYPASS},
        {"unlockVaults", "Unlock Vaults", &c.unlockVaults, "", W_BYPASS},
        {"treasureRoom", "Unlock Treasure Room", &c.treasureRoom, "", W_BYPASS},
        {"mainLevels", "Unlock Main Levels", &c.mainLevels, "", W_BYPASS},
        {"customObjBypass", "Custom Object Bypass", &c.customObjBypass, "Custom objects over 1,000 objects.", W_BYPASS},
        {"sliderLimit", "Slider Limit Bypass", &c.sliderLimit, "", W_BYPASS},
        {"charFilter", "Character Filter Bypass", &c.charFilter, "", W_BYPASS},
        {"textLength", "Text Length Bypass", &c.textLength, "", W_BYPASS},
        {"checkpointLimit", "Checkpoint Limit Bypass", &c.checkpointLimit, "", W_BYPASS},
        {"verifyBypass", "Level Verify Bypass", &c.verifyBypass, "", W_BYPASS},

        {"accuratePercent", "Accurate Percentage", &c.accuratePercent, "Decimals on the in-game percentage.", W_LEVEL},
        {"autoPickupCoins", "Auto Pickup Coins", &c.autoPickupCoins, "Collects every coin at the start of each attempt (not in practice).", W_LEVEL},
        {"autoPractice", "Auto Practice Mode", &c.autoPractice, "", W_LEVEL},

        {"rgbIcons", "Rainbow Icons", &c.rgbIcons, "", W_COSMETIC},
        {"noclipFlash", "Noclip Death Flash", &c.noclipFlash, "", W_COSMETIC},
        {"showWatermark", "Watermark", &c.showWatermark, "", W_COSMETIC},

        {"bufferEnabled", "Input Buffer", &c.bufferEnabled, "", -1},
        {"autoclicker", "Autoclicker", &c.autoclicker, "", -1},
        {"frameStepper", "Frame Stepper", &c.frameStepper, "", -1},
        {"labels", "Labels", &c.labels, "", -1},
        {"macroIgnoreInput", "Ignore my clicks while playing", &c.macroIgnoreInput, "", -1},
    };
    return list;
}

Hack* findHack(const std::string& k) {
    for (auto& h : hacks()) if (k == h.key) return &h;
    return nullptr;
}

std::string keyName(int k) {
    if (!k) return "none";
    if ((k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9')) return std::string(1, char(k));
    if (k >= 0x70 && k <= 0x7B) return "F" + std::to_string(k - 0x6F);
    switch (k) {
        case 0x09: return "Tab"; case 0x20: return "Space"; case 0x1B: return "Esc"; case 0x0D: return "Enter";
        case 0x10: return "Shift"; case 0x11: return "Ctrl"; case 0x12: return "Alt";
        case 0x2D: return "Insert"; case 0x2E: return "Delete"; case 0x24: return "Home"; case 0x23: return "End";
    }
    return "key " + std::to_string(k);
}

void onToggled(const std::string& key) {
    if (key == "fpsUnlock" && cfg().fpsUnlock) CCDirector::sharedDirector()->setAnimationInterval(1.0 / cfg().fps);
    g_dirty = true;
}

void Tooltip(const char* t) {
    if (t && *t && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", t);
}

void Toggle(Hack& h) {
    bool changed = ImGui::Checkbox(h.label, h.value);
    bool rclick = ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    Tooltip(h.tip);
#ifdef GEODE_IS_DESKTOP
    int bind = cfg().binds[h.key];
    if (bind) { ImGui::SameLine(); ImGui::TextDisabled("[%s]", keyName(bind).c_str()); }
    std::string popup = std::string("bind##") + h.key;
    if (rclick) ImGui::OpenPopup(popup.c_str());
    if (ImGui::BeginPopup(popup.c_str())) {
        ImGui::Text("Keybind for %s", h.label);
        if (g_capture == h.key) ImGui::TextColored({1, 0.8f, 0.3f, 1}, "Press a key... (Esc = clear)");
        else if (ImGui::Button("Set key")) g_capture = h.key;
        ImGui::SameLine();
        if (ImGui::Button("Clear")) { cfg().binds[h.key] = 0; g_capture.clear(); g_dirty = true; }
        ImGui::EndPopup();
    } else if (g_capture == h.key) g_capture.clear();
#else
    (void)rclick;
#endif
    if (changed) onToggled(h.key);
}

void ToggleKey(const char* k) { if (auto h = findHack(k)) Toggle(*h); }
void drawTable(int w) { for (auto& h : hacks()) if (h.window == w) Toggle(h); }

void applyStyle() {
    auto& c = cfg();
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    s.WindowRounding = 10.f; s.FrameRounding = 6.f; s.PopupRounding = 8.f; s.GrabRounding = 6.f;
    s.WindowPadding = ImVec2(12, 10); s.FramePadding = ImVec2(8, 5); s.ItemSpacing = ImVec2(8, 6);
    s.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    ImVec4 a(c.accent[0], c.accent[1], c.accent[2], 1.f);
    auto mul = [](ImVec4 v, float m, float al) { return ImVec4(v.x * m, v.y * m, v.z * m, al); };
    ImVec4* col = s.Colors;
    col[ImGuiCol_Text] = ImVec4(0.93f, 0.92f, 0.96f, 1.f);
    col[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.53f, 0.62f, 1.f);
    col[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.06f, 0.10f, 0.94f);
    col[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.08f, 0.13f, 0.98f);
    col[ImGuiCol_Border] = mul(a, 0.6f, 0.45f);
    col[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.12f, 0.20f, 1.f);
    col[ImGuiCol_FrameBgHovered] = mul(a, 0.45f, 1.f);
    col[ImGuiCol_FrameBgActive] = mul(a, 0.6f, 1.f);
    col[ImGuiCol_TitleBg] = mul(a, 0.35f, 1.f);
    col[ImGuiCol_TitleBgActive] = mul(a, 0.55f, 1.f);
    col[ImGuiCol_TitleBgCollapsed] = mul(a, 0.3f, 0.8f);
    col[ImGuiCol_CheckMark] = ImVec4(1, 1, 1, 1);
    col[ImGuiCol_SliderGrab] = a;
    col[ImGuiCol_SliderGrabActive] = mul(a, 1.2f, 1.f);
    col[ImGuiCol_Button] = mul(a, 0.5f, 1.f);
    col[ImGuiCol_ButtonHovered] = mul(a, 0.7f, 1.f);
    col[ImGuiCol_ButtonActive] = a;
    col[ImGuiCol_Header] = mul(a, 0.45f, 1.f);
    col[ImGuiCol_HeaderHovered] = mul(a, 0.6f, 1.f);
    col[ImGuiCol_HeaderActive] = a;
    col[ImGuiCol_Separator] = mul(a, 0.5f, 0.5f);
#ifdef GEODE_IS_MOBILE
    float scale = c.uiScale * 1.7f;   // finger-sized
    s.TouchExtraPadding = ImVec2(4, 4);
#else
    float scale = c.uiScale;
#endif
    s.ScaleAllSizes(scale);
    s.FontScaleMain = scale;
}

void beginWin(const char* name, float x, float y, float w) {
    ImGuiCond cond = g_resetLayout ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
    ImGui::SetNextWindowPos(ImVec2(x, y), cond);
    ImGui::SetNextWindowSize(ImVec2(w, 0), cond);
    ImGui::Begin(name, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
}

void drawMenu() {
    auto& c = cfg();
    auto& s = st();
    float sc = ImGui::GetStyle().FontScaleMain;
    float col = 225.f * sc, gap = 12.f * sc, x0 = 14.f * sc, y0 = 56.f * sc, cx = col + gap;
    ImGuiIO& io = ImGui::GetIO();

    // top bar
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, 8.f), ImGuiCond_Always, ImVec2(0.5f, 0.f));
    ImGui::Begin("##top", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored(ImVec4(c.accent[0], c.accent[1], c.accent[2], 1), "%s", kTitle);
    ImGui::SameLine();
    ImGui::TextDisabled("%s | %s", kVersion, GEODE_PLATFORM_NAME);
    ImGui::SameLine();
    if (safeModeOn()) ImGui::TextColored({0.4f, 1, 0.5f, 1}, " SAFE MODE");
    else if (cheatsActive()) ImGui::TextColored({1, 0.4f, 0.4f, 1}, " CHEATING (progress saves!)");
#ifdef GEODE_IS_MOBILE
    ImGui::SameLine();
    if (ImGui::Button("Close")) { s.menuOpen = false; c.save(); }
#endif
    ImGui::End();

    beginWin("Player", x0, y0, col);
    drawTable(W_PLAYER);
    if (c.noclip) { ImGui::Checkbox("P1##nc", &c.noclipP1); ImGui::SameLine(); ImGui::Checkbox("P2##nc", &c.noclipP2); }
    if (c.showPath) { ImGui::PushItemWidth(-1); ImGui::SliderInt("##plen", &c.pathPreviewTicks, 30, 480, "preview %d ticks"); ImGui::PopItemWidth(); }
    ImGui::End();

    beginWin("Global", x0 + cx, y0, col);
    drawTable(W_GLOBAL);
    ImGui::PushItemWidth(-1);
    if (ImGui::SliderFloat("##speed", &c.speed, 0.05f, 4.f, "speed %.2fx")) g_dirty = true;
    if (ImGui::SliderFloat("##fps", &c.fps, 30.f, 1000.f, "fps %.0f", ImGuiSliderFlags_Logarithmic)) g_dirty = true;
    if (ImGui::IsItemDeactivatedAfterEdit() && c.fpsUnlock) CCDirector::sharedDirector()->setAnimationInterval(1.0 / c.fps);
    ImGui::PopItemWidth();
    ImGui::End();

    beginWin("Bypass", x0 + 2 * cx, y0, col);
    drawTable(W_BYPASS);
    ImGui::End();

    beginWin("Level", x0 + 3 * cx, y0, col);
    drawTable(W_LEVEL);
    ImGui::End();

    beginWin("Cosmetic", x0 + 3 * cx, y0 + 200.f * sc, col);
    drawTable(W_COSMETIC);
    if (c.rgbIcons) { ImGui::PushItemWidth(-1); ImGui::SliderFloat("##rgb", &c.rgbSpeed, 0.05f, 3.f, "rainbow speed %.2f"); ImGui::PopItemWidth(); }
    ImGui::End();

    // input buffer
    beginWin("Input Buffer", x0, y0 + 340.f * sc, col + 40 * sc);
    ToggleKey("bufferEnabled");
    Tooltip("Simulates your click with GD's own physics. If clicking now would kill you,\n"
            "the click is held back and re-checked every physics tick until it survives,\n"
            "then sent with the same hold length you used.");
    float fps = io.Framerate > 1 ? io.Framerate : 60.f;
    ImGui::PushItemWidth(-1);
    if (ImGui::SliderInt("##pdelay", &c.pathMaxDelayMs, 20, 400, "hold back up to %d ms")) g_dirty = true;
    ImGui::PopItemWidth();
    ImGui::Checkbox("Cube / ball / robot / spider", &c.pathCubeModes);
    ImGui::Checkbox("Wave / ship / UFO / swing", &c.pathFlyModes);
    ImGui::Checkbox("Keep early air clicks for orbs", &c.pathHoldAir);
    if (ImGui::CollapsingHeader("Advanced##buf")) {
        ImGui::PushItemWidth(-1);
        ImGui::SliderInt("##plook", &c.pathLookahead, 40, 480, "jump lookahead %d ticks");
        ImGui::SliderInt("##pfly", &c.pathFlyLookahead, 4, 120, "wave/ship lookahead %d ticks");
        ImGui::SliderInt("##phold", &c.pathAssumeHold, 1, 40, "assumed hold %d ticks");
        ImGui::SliderInt("##porb", &c.holdWindowMs, 20, 400, "orb hold up to %d ms");
        ImGui::PopItemWidth();
        ImGui::Checkbox("P1##buf", &c.bufferP1); ImGui::SameLine(); ImGui::Checkbox("P2##buf", &c.bufferP2);
        ImGui::Checkbox("Prediction check (log)", &c.bufferDiagnostics);
    }
    ImGui::TextDisabled("last held back: %d ticks (%.1f frames)", buffer::lastDelayTicks, buffer::lastDelayTicks * fps / 240.f);
    ImGui::TextDisabled("saved: %d  sent anyway: %d  orb holds: %d%s", buffer::savedClicks, buffer::forcedClicks,
                        buffer::heldForOrbs, buffer::pending ? "  [holding]" : "");
    if (!buffer::status.empty()) ImGui::TextDisabled("%s", buffer::status.c_str());
    ImGui::End();

    // tools
    beginWin("Tools", x0 + cx + 40 * sc, y0 + 340.f * sc, col);
    ToggleKey("autoclicker");
    ImGui::PushItemWidth(-1);
    ImGui::SliderInt("##ach", &c.acHold, 1, 30, "hold %d frames");
    ImGui::SliderInt("##acg", &c.acGap, 1, 30, "release %d frames");
    ImGui::PopItemWidth();
    ImGui::Checkbox("P1##ac", &c.acP1); ImGui::SameLine(); ImGui::Checkbox("P2##ac", &c.acP2);
    ImGui::Separator();
    ToggleKey("frameStepper");
    if (c.frameStepper) {
        if (ImGui::Button("Step 1 frame")) s.stepsQueued++;
        ImGui::SameLine();
        if (ImGui::Button("Step 10")) s.stepsQueued += 10;
#ifdef GEODE_IS_DESKTOP
        ImGui::TextDisabled("or press %s", keyName(c.stepKey).c_str());
#endif
    }
    ImGui::End();

    // macro
    beginWin("Macro", x0 + 2 * cx + 40 * sc, y0 + 340.f * sc, col + 20 * sc);
    {
        auto& m = macro();
        const char* names[] = {"idle", "RECORDING", "PLAYING"};
        ImGui::Text("%s", names[m.mode]);
        ImGui::SameLine();
        uint32_t frame = PlayLayer::get() ? frameOf(PlayLayer::get()) : 0;
        ImGui::TextDisabled("%zu inputs | frame %u", m.actions.size(), frame);
        if (m.mode == Macro::Record) { if (ImGui::Button("Stop##r")) m.stop(); } else if (ImGui::Button("Record")) m.startRecord();
        ImGui::SameLine();
        if (m.mode == Macro::Play) { if (ImGui::Button("Stop##p")) m.stop(); } else if (ImGui::Button("Play")) m.startPlay();
        ImGui::SameLine();
        if (ImGui::Button("Clear")) m.clear();
        char nb[64];
        snprintf(nb, sizeof nb, "%s", m.name.c_str());
        ImGui::PushItemWidth(-70 * sc);
        if (ImGui::InputText("##mname", nb, sizeof nb)) m.name = nb;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Save")) m.save();
        static std::vector<std::string> files;
        static int sel = -1;
        if (ImGui::Button("Refresh") || (files.empty() && ImGui::IsWindowAppearing())) { files = m.list(); sel = -1; }
        if (ImGui::BeginListBox("##macros", ImVec2(-1, 90 * sc))) {
            for (int i = 0; i < (int)files.size(); i++)
                if (ImGui::Selectable(files[i].c_str(), sel == i)) sel = i;
            ImGui::EndListBox();
        }
        if (sel >= 0 && sel < (int)files.size() && ImGui::Button("Load selected")) m.load(files[sel]);
        ToggleKey("macroIgnoreInput");
        if (!m.status.empty()) ImGui::TextWrapped("%s", m.status.c_str());
        ImGui::TextDisabled("Tip: turn off \"Click Between Steps\" for exact playback.");
    }
    ImGui::End();

    // labels + settings
    beginWin("Labels", x0 + 3 * cx + 40 * sc, y0 + 340.f * sc, col);
    ToggleKey("labels");
    const char* corners[] = {"Top left", "Top right", "Bottom left", "Bottom right"};
    ImGui::PushItemWidth(-1);
    ImGui::Combo("##corner", &c.labelCorner, corners, 4);
    ImGui::SliderFloat("##ls", &c.labelScale, 0.5f, 2.5f, "size %.2f");
    ImGui::SliderFloat("##la", &c.labelAlpha, 0.1f, 1.f, "opacity %.2f");
    ImGui::PopItemWidth();
    ImGui::Checkbox("Cheat indicator", &c.lblCheat);
    ImGui::Checkbox("FPS", &c.lblFps); ImGui::SameLine(); ImGui::Checkbox("CPS", &c.lblCps);
    ImGui::Checkbox("Clicks", &c.lblClicks); ImGui::SameLine(); ImGui::Checkbox("Percent", &c.lblPercent);
    ImGui::Checkbox("Attempts", &c.lblAttempts); ImGui::SameLine(); ImGui::Checkbox("Clock", &c.lblClock);
    ImGui::Checkbox("Noclip accuracy", &c.lblNoclipAcc);
    ImGui::Checkbox("Noclip deaths", &c.lblNoclipDeaths);
    ImGui::Checkbox("Session time", &c.lblSession);
    ImGui::Checkbox("Input buffer", &c.lblBuffer);
    ImGui::Checkbox("Custom text", &c.lblCustom);
    if (c.lblCustom) {
        char buf[128];
        snprintf(buf, sizeof buf, "%s", c.customText.c_str());
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##ct", buf, sizeof buf)) c.customText = buf;
        ImGui::PopItemWidth();
    }
    ImGui::Separator();
    ImGui::PushItemWidth(-1);
    if (ImGui::ColorEdit3("##accent", c.accent, ImGuiColorEditFlags_NoInputs)) applyStyle();
    ImGui::SameLine(); ImGui::Text("Accent");
    ImGui::SliderFloat("##uis", &c.uiScale, 0.6f, 2.0f, "menu size %.2f");
    if (ImGui::IsItemDeactivatedAfterEdit()) applyStyle();
    ImGui::PopItemWidth();
#ifdef GEODE_IS_DESKTOP
    ImGui::Text("Menu key: %s", keyName(c.menuKey).c_str());
    if (g_capture == "__menu") ImGui::TextColored({1, 0.8f, 0.3f, 1}, "Press a key...");
    else if (ImGui::Button("Change menu key")) g_capture = "__menu";
    ImGui::TextDisabled("Right-click a toggle to bind a key.");
#endif
    if (ImGui::Button("Save")) { c.save(); g_dirty = false; }
    ImGui::SameLine();
    if (ImGui::Button("Reset layout")) g_resetLayout = 2;
    ImGui::End();
}

void drawOverlay() {
    auto& c = cfg();
    auto& s = st();
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (s.flash > 0.f) {
        dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(255, 0, 0, int(s.flash * 255)));
        s.flash -= io.DeltaTime;
    }
    double now = nowSeconds();
    while (!s.clickTimes.empty() && now - s.clickTimes.front() > 1.0) s.clickTimes.pop_front();

    if (now < g_toastUntil) {
#ifdef GEODE_IS_MOBILE
        const char* t = "insirius's mod menu loaded - tap the menu button";
#else
        std::string ts = std::string(kTitle) + " loaded - press " + keyName(c.menuKey);
        const char* t = ts.c_str();
#endif
        float fs = ImGui::GetFontSize() * 1.1f;
        ImVec2 sz = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0, t);
        ImVec2 p((io.DisplaySize.x - sz.x) * 0.5f, io.DisplaySize.y * 0.85f);
        dl->AddRectFilled(ImVec2(p.x - 14, p.y - 8), ImVec2(p.x + sz.x + 14, p.y + sz.y + 8), IM_COL32(20, 14, 32, 220), 10.f);
        dl->AddText(ImGui::GetFont(), fs, p, IM_COL32_WHITE, t);
    }

    auto pl = PlayLayer::get();
    if (c.showWatermark && !s.menuOpen) {
        float fs = ImGui::GetFontSize() * 0.85f;
        ImVec2 sz = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0, kTitle);
        dl->AddText(ImGui::GetFont(), fs, ImVec2(io.DisplaySize.x - sz.x - 8, io.DisplaySize.y - sz.y - 6), IM_COL32(255, 255, 255, 70), kTitle);
    }
    if (!c.labels || !pl) return;

    struct Line { std::string text; ImU32 color; };
    std::vector<Line> lines;
    ImU32 white = IM_COL32_WHITE;
    if (c.lblCheat) {
        bool cheat = cheatsActive(), safe = safeModeOn();
        lines.push_back({cheat ? (safe ? "cheating (safe mode)" : "CHEATING") : "legit",
                         cheat ? (safe ? IM_COL32(255, 200, 80, 255) : IM_COL32(255, 80, 80, 255)) : IM_COL32(110, 255, 140, 255)});
    }
    if (c.lblFps) lines.push_back({fmt::format("{:.0f} FPS", io.Framerate), white});
    if (c.lblCps) lines.push_back({fmt::format("{} CPS", s.clickTimes.size()), white});
    if (c.lblClicks) lines.push_back({fmt::format("{} clicks", s.totalClicks), white});
    if (c.lblPercent && pl->m_player1) lines.push_back({fmt::format("{:.2f}%", pl->getCurrentPercent()), white});
    if (c.lblAttempts) lines.push_back({fmt::format("Attempt {}{}", s.attempts, pl->m_isPracticeMode ? " (practice)" : ""), white});
    if (c.noclip && c.lblNoclipAcc) {
        float acc = s.frames ? 100.f * (1.f - float(s.deathFrames) / float(s.frames)) : 100.f;
        lines.push_back({fmt::format("Accuracy {:.2f}%", acc), s.diedLastFrame ? IM_COL32(255, 90, 90, 255) : white});
    }
    if (c.noclip && c.lblNoclipDeaths) lines.push_back({fmt::format("Deaths {}", s.noclipDeaths), white});
    if (c.lblSession) {
        int secs = int(now - s.sessionStart);
        lines.push_back({fmt::format("Session {}:{:02}:{:02}", secs / 3600, secs / 60 % 60, secs % 60), white});
    }
    if (c.lblClock) {
        time_t t = time(nullptr);
        char buf[16];
        strftime(buf, sizeof buf, "%H:%M:%S", localtime(&t));
        lines.push_back({buf, white});
    }
    if (c.bufferEnabled && c.lblBuffer)
        lines.push_back({buffer::pending ? "Buffer [holding]" : "Buffer", buffer::pending ? IM_COL32(190, 140, 255, 255) : white});
    if (c.frameStepper) lines.push_back({"Frame stepper", IM_COL32(255, 200, 80, 255)});
    if (macro().mode == Macro::Record) lines.push_back({"Macro: recording", IM_COL32(255, 90, 90, 255)});
    if (macro().mode == Macro::Play) lines.push_back({"Macro: playing", IM_COL32(110, 255, 140, 255)});
    if (c.lblCustom && !c.customText.empty()) lines.push_back({c.customText, white});

    float fs = ImGui::GetFontSize() * c.labelScale, lh = fs * 1.15f;
    bool right = c.labelCorner == 1 || c.labelCorner == 3, bottom = c.labelCorner >= 2;
    float y = bottom ? io.DisplaySize.y - 8 - lh * lines.size() : 6;
    int a = int(c.labelAlpha * 255);
    for (auto& l : lines) {
        ImVec2 sz = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0, l.text.c_str());
        float x = right ? io.DisplaySize.x - sz.x - 8 : 8;
        dl->AddText(ImGui::GetFont(), fs, ImVec2(x + 1.5f, y + 1.5f), IM_COL32(0, 0, 0, a * 2 / 3), l.text.c_str());
        dl->AddText(ImGui::GetFont(), fs, ImVec2(x, y), (l.color & 0x00FFFFFF) | (ImU32(a) << 24), l.text.c_str());
        y += lh;
    }
}

#ifdef GEODE_IS_MOBILE
void drawOpenButton() {
    auto& c = cfg();
    auto& s = st();
    ImGui::SetNextWindowPos(ImVec2(c.buttonX, c.buttonY), ImGuiCond_Once);
    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGui::Begin("##openbtn", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
    if (ImGui::Button(s.menuOpen ? "close" : "insirius")) {
        s.menuOpen = !s.menuOpen;
        if (!s.menuOpen) c.save();
    }
    ImVec2 pos = ImGui::GetWindowPos();
    if (pos.x != c.buttonX || pos.y != c.buttonY) { c.buttonX = pos.x; c.buttonY = pos.y; g_dirty = true; }
    ImGui::End();
}
#endif

void render() {
    auto& s = st();
#ifdef GEODE_IS_MOBILE
    drawOpenButton();
#endif
    if (s.menuOpen) {
        drawMenu();
        if (g_resetLayout) g_resetLayout--;
    }
    drawOverlay();
    double now = nowSeconds();
    if (g_dirty && now - g_lastSave > 5.0) { cfg().save(); g_dirty = false; g_lastSave = now; }
}

bool handleKey(int key, bool down, bool repeat) {
    auto& c = cfg();
    auto& s = st();
    if (!down) return false;
    if (!g_capture.empty()) {
        if (repeat) return true;
        if (g_capture == "__menu") { if (key != 0x1B) c.menuKey = key; }
        else c.binds[g_capture] = key == 0x1B ? 0 : key;
        g_capture.clear();
        g_dirty = true;
        return true;
    }
    if (key == c.menuKey) {
        if (!repeat) { s.menuOpen = !s.menuOpen; if (!s.menuOpen) c.save(); }
        return true;
    }
    if (s.menuOpen && ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput) return false;
    if (c.frameStepper && key == c.stepKey && PlayLayer::get()) { s.stepsQueued++; return true; }
    if (repeat) return false;
    bool used = false;
    for (auto& [k, b] : c.binds) {
        if (b != key) continue;
        if (auto h = findHack(k)) { *h->value = !*h->value; onToggled(k); used = true; }
    }
    return used;
}
} // namespace

class $modify(InsKeyboard, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double time) {
#ifdef GEODE_IS_DESKTOP
        if (handleKey(static_cast<int>(key), down, repeat)) return true;
#endif
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, time);
    }
};

class $modify(InsMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        static bool once = false;
        if (!once) {
            once = true;
            g_toastUntil = nowSeconds() + 6.0;
        }
        return true;
    }
};

$on_mod(Loaded) {
    cfg().load();
    ImGuiCocos::get().setForceLegacy(Mod::get()->getSettingValue<bool>("legacy-render"));
    ImGuiCocos::get()
        .setup([] {
            ImGui::GetIO().IniFilename = nullptr;   // window positions come from our layout
            applyStyle();
        })
        .draw([] { render(); });
    if (cfg().fpsUnlock) CCDirector::sharedDirector()->setAnimationInterval(1.0 / cfg().fps);
}
