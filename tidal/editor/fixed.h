// Tracker Cycles. Copyright (C) 2026 CyopStudio and Tracker Cycles contributors.
// GPL-3.0-or-later, with an additional permission for Polyend Tracker CE (tidal/LICENSE).
// Fixed-capacity string and vector for the editor model (plan 4d): the
// subset of std::string / std::vector the editor uses, no heap. Overflow
// truncates (strings) or drops (vectors) silently; `full()` tells.
#pragma once
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <initializer_list>

namespace tc {

template <int N>
class Str {
public:
    static constexpr int npos = -1;
    Str() { s_[0] = 0; }
    Str(const char* s) { assign(s); }
    Str(const char* s, int n) { assign(s, n); }
    template <int M>
    Str(const Str<M>& o) {
        assign(o.c_str());
        over_ = over_ || o.overflowed();
    }

    Str& assign(const char* s) { return assign(s, s ? (int)strlen(s) : 0); }
    Str& assign(const char* s, int n) {
        over_ = n > N;
        if (n > N) n = N;
        if (n > 0) memmove(s_, s, (size_t)n);
        n_ = n > 0 ? n : 0;
        s_[n_] = 0;
        return *this;
    }
    Str& operator=(const char* s) { return assign(s); }
    template <int M>
    Str& operator=(const Str<M>& o) { return assign(o.c_str()); }

    const char* c_str() const { return s_; }
    int size() const { return n_; }
    bool empty() const { return n_ == 0; }
    bool full() const { return n_ == N; }
    bool overflowed() const { return over_; }  // something was cut off since the last assign
    static constexpr int capacity() { return N; }
    char operator[](int i) const { return s_[i]; }
    char& operator[](int i) { return s_[i]; }
    char back() const { return n_ ? s_[n_ - 1] : 0; }
    char front() const { return s_[0]; }
    void clear() { n_ = 0, s_[0] = 0, over_ = false; }
    void pop_back() {
        if (n_) s_[--n_] = 0;
    }
    void resize(int n) {
        if (n < n_) n_ = n < 0 ? 0 : n, s_[n_] = 0;
    }

    Str& append(const char* s, int n) {
        if (n > N - n_) n = N - n_, over_ = true;
        if (n > 0) memcpy(s_ + n_, s, (size_t)n), n_ += n;
        s_[n_] = 0;
        return *this;
    }
    Str& operator+=(const char* s) { return append(s, (int)strlen(s)); }
    Str& operator+=(char c) { return append(&c, 1); }
    template <int M>
    Str& operator+=(const Str<M>& o) { return append(o.c_str(), o.size()); }

    // position of `s` (or char c), npos if none
    int find(const char* s, int from = 0) const {
        if (from > n_) return npos;
        const char* p = strstr(s_ + from, s);
        return p ? (int)(p - s_) : npos;
    }
    int find(char c, int from = 0) const {
        for (int i = from; i < n_; i++)
            if (s_[i] == c) return i;
        return npos;
    }
    int rfind(char c) const {
        for (int i = n_ - 1; i >= 0; i--)
            if (s_[i] == c) return i;
        return npos;
    }
    bool startsWith(const char* p) const { return strncmp(s_, p, strlen(p)) == 0; }
    Str substr(int pos, int len = N) const {
        Str r;
        if (pos < 0 || pos >= n_) return r;
        if (len > n_ - pos) len = n_ - pos;
        return r.assign(s_ + pos, len), r;
    }
    void erase(int pos, int len = N) {
        if (pos < 0 || pos >= n_) return;
        if (len > n_ - pos) len = n_ - pos;
        memmove(s_ + pos, s_ + pos + len, (size_t)(n_ - pos - len + 1));
        n_ -= len;
    }
    void insert(int pos, char c) {
        if (pos < 0 || pos > n_ || n_ >= N) return;
        memmove(s_ + pos + 1, s_ + pos, (size_t)(n_ - pos + 1));
        s_[pos] = c;
        n_++;
    }

    bool operator==(const char* o) const { return strcmp(s_, o) == 0; }
    bool operator!=(const char* o) const { return strcmp(s_, o) != 0; }
    template <int M>
    bool operator==(const Str<M>& o) const { return strcmp(s_, o.c_str()) == 0; }
    template <int M>
    bool operator!=(const Str<M>& o) const { return strcmp(s_, o.c_str()) != 0; }

    Str& printf(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

private:
    char s_[N + 1];
    int n_ = 0;
    bool over_ = false;
};

template <int N>
Str<N>& Str<N>::printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int k = vsnprintf(s_, (size_t)N + 1, fmt, ap);
    va_end(ap);
    n_ = k < 0 ? 0 : k > N ? N : k;
    over_ = k > N;
    return *this;
}

// The editor's working text (codes, rendered steps, clipboard) and the
// short kind (labels, titles, the message line).
using Text = Str<512>;
using Label = Str<128>;

template <int N>
Str<N> operator+(const Str<N>& a, const char* b) {
    Str<N> r = a;
    return r += b, r;
}
template <int N, int M>
Str<N> operator+(const Str<N>& a, const Str<M>& b) {
    Str<N> r = a;
    return r += b, r;
}
template <int N>
Str<N> operator+(const Str<N>& a, char c) {
    Str<N> r = a;
    return r += c, r;
}
template <int N>
Str<N> operator+(const char* a, const Str<N>& b) {
    Str<N> r = a;
    return r += b, r;
}
inline Text tnum(long v) {
    Text r;
    return r.printf("%ld", v), r;
}

template <class T, int N>
class FixVec {
public:
    FixVec() {}
    FixVec(std::initializer_list<T> l) { *this = l; }
    FixVec& operator=(std::initializer_list<T> l) {
        n_ = 0;
        for (const T& v : l) push_back(v);
        return *this;
    }
    int size() const { return n_; }
    bool empty() const { return n_ == 0; }
    bool full() const { return n_ == N; }
    static constexpr int capacity() { return N; }
    void clear() { n_ = 0; }
    bool push_back(const T& v) {
        if (n_ >= N) return false;
        a_[n_++] = v;
        return true;
    }
    void pop_back() {
        if (n_) n_--;
    }
    void erase_at(int i) {
        if (i < 0 || i >= n_) return;
        for (int k = i; k + 1 < n_; k++) a_[k] = a_[k + 1];
        n_--;
    }
    T& operator[](int i) { return a_[i]; }
    const T& operator[](int i) const { return a_[i]; }
    T& back() { return a_[n_ - 1]; }
    const T& back() const { return a_[n_ - 1]; }
    T* begin() { return a_; }
    T* end() { return a_ + n_; }
    const T* begin() const { return a_; }
    const T* end() const { return a_ + n_; }

private:
    T a_[N];
    int n_ = 0;
};

}  // namespace tc
