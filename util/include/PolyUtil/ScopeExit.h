#pragma once

/**
 * Utility helper to assure resource release on scope end.
 */
template<typename F>
struct ScopeExit
{
    explicit ScopeExit(F f)
      : f_(f)
    {
    }

    ScopeExit(const ScopeExit&) = delete;
    ScopeExit(ScopeExit&&) noexcept = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
    ScopeExit& operator=(ScopeExit&&) = delete;

    ~ScopeExit() { f_(); }

  private:
    F f_;
};
