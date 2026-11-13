// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Loader for the golden files (golden/cases.txt, golden/expected.txt).
// Test-only host code: uses the standard library freely. The engine's
// golden comparison will use these structs once the query engine exists.
#pragma once
#include <stdlib.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace golden {

struct Rat {
    long long num = 0, den = 1;
};

struct Value {
    std::string key;
    char tag = '?';     // s string, f double, n note, r rational, i int, b bool
    std::string text;   // as printed by Tidal
};

struct Event {
    bool hasWhole = false;  // false: continuous signal ("~ ~")
    Rat ws, we, ps, pe;     // whole start/stop, part start/stop
    std::vector<Value> values;
};

struct Case {
    std::string name, code;
    int cycles = 0, line = 0;
    std::vector<Event> events;
};

inline bool parseRat(const std::string& s, Rat& r) {
    size_t slash = s.find('/');
    if (slash == std::string::npos) return false;
    char* end;
    r.num = strtoll(s.c_str(), &end, 10);
    if (end != s.c_str() + slash) return false;
    r.den = strtoll(s.c_str() + slash + 1, &end, 10);
    return *end == 0 && r.den > 0;
}

// "name cycles code" -> case; false if the line isn't one.
inline bool parseHeader(const std::string& line, Case& c) {
    std::istringstream in(line);
    if (!(in >> c.name >> c.cycles)) return false;
    std::getline(in, c.code);
    size_t b = c.code.find_first_not_of(" \t");
    size_t e = c.code.find_last_not_of(" \t\r");
    if (b == std::string::npos) return false;
    c.code = c.code.substr(b, e - b + 1);
    return true;
}

inline bool loadCases(const char* path, std::vector<Case>& out, std::string& err) {
    std::ifstream f(path);
    if (!f) return err = std::string("can't open ") + path, false;
    std::string line;
    int ln = 0;
    while (std::getline(f, line)) {
        ln++;
        size_t b = line.find_first_not_of(" \t");
        if (b == std::string::npos || line[b] == '#') continue;
        Case c;
        if (!parseHeader(line, c)) return err = std::string(path) + ":" + std::to_string(ln) + ": bad case", false;
        c.line = ln;
        out.push_back(c);
    }
    return true;
}

inline bool parseEvent(const std::string& line, Event& e) {
    std::istringstream in(line);
    std::string t[4];
    for (auto& x : t)
        if (!(in >> x)) return false;
    e.hasWhole = t[0] != "~";
    if (e.hasWhole && (!parseRat(t[0], e.ws) || !parseRat(t[1], e.we))) return false;
    if (!e.hasWhole && t[1] != "~") return false;
    if (!parseRat(t[2], e.ps) || !parseRat(t[3], e.pe)) return false;
    std::string kv;
    while (in >> kv) {
        size_t eq = kv.find('='), colon = kv.find(':', eq);
        if (eq == std::string::npos || colon != eq + 2) return false;
        Value v;
        v.key = kv.substr(0, eq);
        v.tag = kv[eq + 1];
        v.text = kv.substr(colon + 1);
        e.values.push_back(v);
    }
    return true;
}

inline bool loadExpected(const char* path, std::vector<Case>& out, std::string& err) {
    std::ifstream f(path);
    if (!f) return err = std::string("can't open ") + path, false;
    std::string line;
    int ln = 0;
    while (std::getline(f, line)) {
        ln++;
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("== ", 0) == 0) {
            Case c;
            if (!parseHeader(line.substr(3), c))
                return err = std::string(path) + ":" + std::to_string(ln) + ": bad header", false;
            c.line = ln;
            out.push_back(c);
            continue;
        }
        Event e;
        if (out.empty() || !parseEvent(line, e))
            return err = std::string(path) + ":" + std::to_string(ln) + ": bad event", false;
        out.back().events.push_back(e);
    }
    return true;
}

}  // namespace golden
