#pragma once

#include <cassert>

/**
 * Simple wrapper for a pair of value and status code.
 * Default value of the status type (0 in case of enum class)
 * is intepreted as "no error" and validates the value.
 * Implicit conversions from/to the Value type and from the Status type.
 */
template<class Value, class Status>
class Result
{
  public:
    Result(Value value) // NOLINT(*-explicit-*)
      : v(value)
      , s(){};

    Result(Status status) // NOLINT(*-explicit-*)
      : v()
      , s(status)
    {
        assert(status != Status{});
    };

    // NOLINTNEXTLINE(*-explicit-*)
    [[nodiscard]] operator bool() const { return s == Status{}; };
    [[nodiscard]] bool error() const { return s != Status{}; };
    [[nodiscard]] bool success() const { return s == Status{}; };
    [[nodiscard]] Status status() const { return s; };
    [[nodiscard]] Value value() const
    {
        assert(success());
        return v;
    };
    [[nodiscard]] Value operator*() const { return value(); };

    [[nodiscard]] const Value& ref() const
    {
        assert(success());
        return v;
    };
    [[nodiscard]] Value& ref()
    {
        assert(success());
        return v;
    }

  private:
    Value v;
    Status s;
};
