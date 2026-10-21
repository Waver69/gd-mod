#include "macro.hpp"
#include "config.hpp"
#include <Geode/Geode.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

Macro& macro() {
    static Macro m;
    return m;
}

std::string Macro::dir() {
    auto p = geode::Mod::get()->getSaveDir() / "macros";
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    return geode::utils::string::pathToString(p);
}

void Macro::startRecord() {
    actions.clear();
    index = 0;
    mode = Record;
    status = "Recording - restart the level to begin from 0";
}

void Macro::startPlay() {
    if (actions.empty()) { status = "Nothing to play - record or load a macro"; return; }
    index = 0;
    mode = Play;
    status = "Playing - restart the level to begin from 0";
}

void Macro::stop() {
    mode = Off;
    status = "Stopped (" + std::to_string(actions.size()) + " inputs)";
}

void Macro::clear() {
    actions.clear();
    index = 0;
    if (mode == Play) mode = Off;
    status = "Cleared";
}

void Macro::onInput(uint32_t frame, bool down, int button, bool p1) {
    if (mode != Record) return;
    // keep it sorted (inputs inside one step can arrive after a later-frame one after a rewind)
    MacroAction a{frame, down, uint8_t(button), p1};
    if (!actions.empty() && actions.back().frame > frame) {
        auto it = std::upper_bound(actions.begin(), actions.end(), frame,
                                   [](uint32_t f, const MacroAction& x) { return f < x.frame; });
        actions.insert(it, a);
    } else {
        actions.push_back(a);
    }
}

static std::string clean(std::string n) {
    std::string out;
    for (char c : n) if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == ' ') out += c;
    if (out.empty()) out = "macro";
    return out.substr(0, 64);
}

bool Macro::save() {
    name = clean(name);
    std::ofstream f(std::filesystem::path(dir()) / (name + ".imacro"), std::ios::trunc);
    if (!f) { status = "Couldn't write file"; return false; }
    f << "IMACRO 1 240\n";
    for (auto& a : actions) f << a.frame << ' ' << (a.down ? 1 : 0) << ' ' << int(a.button) << ' ' << (a.p1 ? 1 : 0) << '\n';
    status = "Saved " + name + " (" + std::to_string(actions.size()) + " inputs)";
    return true;
}

bool Macro::load(const std::string& n) {
    std::ifstream f(std::filesystem::path(dir()) / (clean(n) + ".imacro"));
    if (!f) { status = "Couldn't open " + n; return false; }
    std::string header;
    std::getline(f, header);
    if (header.rfind("IMACRO", 0) != 0) { status = "Not an insirius macro"; return false; }
    actions.clear();
    uint32_t fr; int d, b, p;
    while (f >> fr >> d >> b >> p) actions.push_back({fr, d != 0, uint8_t(b), p != 0});
    std::stable_sort(actions.begin(), actions.end(), [](auto& x, auto& y) { return x.frame < y.frame; });
    name = clean(n);
    index = 0;
    mode = Off;
    status = "Loaded " + name + " (" + std::to_string(actions.size()) + " inputs)";
    return true;
}

std::vector<std::string> Macro::list() {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(dir(), ec)) {
        if (e.path().extension() == ".imacro") out.push_back(geode::utils::string::pathToString(e.path().stem()));
    }
    std::sort(out.begin(), out.end());
    return out;
}
