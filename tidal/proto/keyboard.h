// The Tracker's on-screen keyboard (stock firmware: core/source/
// mtKeyboardManager.h, used for project / instrument names), reproduced so
// comment editing works the way naming does on the device: 42 keys on the
// 48 pads (two pads for Backspace and Caps, five for Space), arrows move the
// highlighted key, Screen 1 / Insert types it. Layout and navigation tables
// are the stock ones; the device editor (4d) can use mtKeyboardManager
// itself.
#pragma once
#include <stdint.h>

namespace proto {
namespace kbd {

constexpr int kKeys = 42;
constexpr char kBackspace = 0, kCaps = 1;
constexpr int kKeyBackspace = 10, kKeyCaps = 33, kKeySpace = 41;

constexpr char kSmall[kKeys] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', kBackspace,
                                'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '-',
                                '+', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '@',
                                kCaps, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ' '};
constexpr char kBig[kKeys] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', kBackspace,
                              'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '-',
                              '+', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '@',
                              kCaps, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ' '};

// highlighted key after an arrow: [left, right, up, down][key]
constexpr uint8_t kMove[4][kKeys] = {
    {10, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  22, 11, 12, 13, 14, 15, 16, 17, 18, 19,
     20, 21, 33, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 41, 34, 35, 36, 37, 38, 39, 40},
    {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 0,  12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
     22, 11, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 23, 35, 36, 37, 38, 39, 40, 41, 34},
    {34, 35, 36, 37, 38, 39, 40, 41, 41, 41, 41, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
     10, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 23, 24, 25, 26, 27, 28, 29, 30},
    {11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
     33, 33, 34, 35, 36, 37, 38, 39, 40, 41, 41, 41, 41, 0,  1,  2,  3,  4,  5,  6,  7},
};

// pad (0..47) -> key, key -> its first pad
constexpr uint8_t kPadKey[48] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 10,
                                 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                                 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 33,
                                 34, 35, 36, 37, 38, 39, 40, 41, 41, 41, 41, 41};
constexpr uint8_t kKeyPad[kKeys] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 12, 13, 14,
                                    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
                                    29, 30, 31, 32, 33, 34, 36, 37, 38, 39, 40, 41, 42, 43};
// pads a key covers
inline int keyWidth(int key) { return key == kKeyBackspace || key == kKeyCaps ? 2 : key == kKeySpace ? 5 : 1; }

// key that types `c` (and whether Caps is needed), -1 if none
inline int keyFor(char c, bool& caps) {
    for (int k = 0; k < kKeys; k++) {
        if (k == kKeyBackspace || k == kKeyCaps) continue;
        if (kSmall[k] == c) return caps = false, k;
        if (kBig[k] == c) return caps = true, k;
    }
    return -1;
}

}  // namespace kbd
}  // namespace proto
