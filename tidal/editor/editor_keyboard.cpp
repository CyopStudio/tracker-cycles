// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// The comment keyboard (Shift+Config): the stock naming keyboard's layout.
#include "editor_impl.h"
#include "cold.h"

TC_COLD_BEGIN
namespace tc {

// ---------------------------------------------------------------------------
// keyboard screen (Params): the block's comment

void Editor::openCommentKeyboard() {
    const uint16_t b = prog_->block[slot_];
    if (!b) return error("no block to comment (start one first)");
    const int idx = prog_->blockIndex(b);
    if (idx < 0) return error("block not in the file");
    const Text all = prog_->blockComment(idx);
    // edit the last line (the one right above the block); earlier lines stay
    const int nl = all.rfind('\n');
    const Text head = nl == Text::npos ? "" : all.substr(0, nl + 1);
    Text last = nl == Text::npos ? all : all.substr(nl + 1);
    if (last.startsWith("--")) last = last.substr(2);
    if (!last.empty() && last[0] == ' ') last = last.substr(1);
    kbdMax_ = std::min(48, kMaxCommentLen - (int)head.size() - 3);
    if (kbdMax_ <= 0) return error("comment too long already");
    if ((int)last.size() > kbdMax_) last.resize(kbdMax_);
    kbdOpen_ = true;
    kbdBlock_ = idx;
    kbdText_ = last;
    kbdPos_ = (int)last.size();
    kbdSel_ = 0;
    kbdCaps_ = false;
    menuOpen_ = false;
    kbdTitle_ = "comment above d" + tnum(slot_ + 1);
    info("keyboard: pads type, arrows + Screen 1 type the lit key, jog moves the cursor, Screen 8 OK, Screen 7 Cancel");
}

void Editor::kbdKey(int key) {
    if (!kbdOpen_ || key < 0 || key >= kbd::kKeys) return;
    kbdSel_ = key;
    if (key == kbd::kKeyCaps) {
        kbdCaps_ = !kbdCaps_;
        return;
    }
    if (key == kbd::kKeyBackspace) {
        if (kbdPos_ > 0) kbdText_.erase(--kbdPos_, 1);
        return;
    }
    if ((int)kbdText_.size() >= kbdMax_) return error("comment full (" + tnum(kbdMax_) + " characters)");
    const char c = kbdCaps_ ? kbd::kBig[key] : kbd::kSmall[key];
    kbdText_.insert(kbdPos_++, c);
    messageError_ = false;
}

void Editor::kbdAction(Action a) {
    switch (a) {
        case Action::Left: kbdSel_ = kbd::kMove[0][kbdSel_]; return;
        case Action::Right: kbdSel_ = kbd::kMove[1][kbdSel_]; return;
        case Action::Up: kbdSel_ = kbd::kMove[2][kbdSel_]; return;
        case Action::Down: kbdSel_ = kbd::kMove[3][kbdSel_]; return;
        case Action::WheelUp:
        case Action::WheelUpFast: kbdPos_ = std::min((int)kbdText_.size(), kbdPos_ + 1); return;
        case Action::WheelDown:
        case Action::WheelDownFast: kbdPos_ = std::max(0, kbdPos_ - 1); return;
        case Action::KbdType:
        case Action::Insert: return kbdKey(kbdSel_);
        case Action::Delete: return kbdKey(kbd::kKeyBackspace);
        case Action::KbdClear:
            kbdText_.clear();
            kbdPos_ = 0;
            return;
        case Action::KbdCancel:
        case Action::Cancel:
            kbdOpen_ = false;
            return info("comment unchanged");
        case Action::KbdOk:
        case Action::Confirm: {
            kbdOpen_ = false;
            if (kbdBlock_ < 0 || kbdBlock_ >= prog_->blockCount()) return error("block gone");
            const Text all = prog_->blockComment(kbdBlock_);
            const int nl = all.rfind('\n');
            Text head = nl == Text::npos ? "" : all.substr(0, nl + 1);
            Text text = kbdText_;
            while (!text.empty() && text.back() == ' ') text.pop_back();
            Text out = text.empty() ? head : head + "-- " + text;
            while (!out.empty() && out.back() == '\n') out.pop_back();
            if (out == all) return info("comment unchanged");
            if ((int)out.size() > kMaxCommentLen) return error("comment too long");
            if (uint8_t* d = reserveUndo('M', 4 + all.size() + 1)) {  // undo step: the comment before
                memcpy(d, &kbdBlock_, sizeof kbdBlock_);
                memcpy(d + 4, all.c_str(), (size_t)all.size() + 1);
            }
            memcpy(prog_->blockComment(kbdBlock_), out.c_str(), out.size() + 1);
            changes_++;
            return info(text.empty() ? "comment removed" : "comment: " + text);
        }
        default: return;  // nothing else while the keyboard is up
    }
}

}  // namespace tc
TC_COLD_END
