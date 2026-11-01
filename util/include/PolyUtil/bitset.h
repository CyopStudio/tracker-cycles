#pragma once

#include <bitset>
#include <cassert>
#include <optional>

#include "misc.h"

namespace bitset
{

/**
 * finds Nth set bit in a bitset starting from given position.
 * @startingPos - starting position
 * @skipCount - number of set bits to skip
 * @up - direction
 * @return last found one index or std::nullopt if not found any
 */
template<size_t Size>
tiny_optional<int> findSetBit(std::bitset<Size> bits, int startingPos, int skipCount, bool up)
{
    assert(skipCount >= 0);
    assert(startingPos < static_cast<int>(Size) && startingPos >= 0);
    auto counter = 0;
    tiny_optional<int> lastFound = std::nullopt;
    if (up)
    {
        for (int i = startingPos; i < static_cast<int>(Size); i++)
        {

            if (bits.test(i))
            {
                lastFound = i;
                if (++counter > skipCount)
                {
                    break;
                }
            }
        }
    }
    else
    {

        for (int i = startingPos; i >= 0; i--)
        {
            if (bits.test(i))
            {
                lastFound = i;
                if (++counter > skipCount)
                {
                    break;
                }
            }
        }
    }
    return lastFound;
}

/**
 * Counts bits in std::bitset starting from @startPos to @endPos inclusive
 */
template<size_t Size>
constexpr size_t countSetBits(std::bitset<Size> bits, size_t startPos, size_t endPos)
{
    assert(startPos <= endPos);
    assert(startPos < Size);
    assert(endPos < Size);

    int result = 0;
    for (size_t i = startPos; i <= endPos; i++)
    {
        if (bits.test(i))
        {
            result++;
        }
    }
    return result;
}

/**
 * expands given bitset in size. Content is duplicated.
 * Ex. expand<4, 9>(std::bitset<4>(0b0011)) == std::bitset<9>(0b100110011)
 */
template<int FromSize, int ToSize>
static std::bitset<ToSize> expand(std::bitset<FromSize> bits)
{
    static_assert(FromSize < ToSize);
    std::bitset<ToSize> expansion{ bits.to_ulong() };
    auto tmp = expansion;
    constexpr int div = ToSize / FromSize;
    for (int i = 0; i < div; i++)
    {
        expansion |= tmp << (FromSize * i);
    }
    constexpr int rest = ToSize % FromSize;
    std::bitset<ToSize> lastBytesMask;
    lastBytesMask = lastBytesMask.set() << (ToSize - rest);
    expansion |= (expansion << (ToSize - rest)) & lastBytesMask;
    return expansion;
}

/**
 * Rotates bitset in left direction. Rotation is a circular shift.
 */
template<size_t Size>
static std::bitset<Size> rotateLeft(std::bitset<Size> bit, int n)
{
    std::bitset<Size> ones = 0;
    auto maskShift = Size - n % Size;
    auto mask = ones.set() << maskShift;
    auto tmp = mask & bit;
    tmp = tmp >> maskShift;
    return (bit << (n % Size)) | tmp;
}

template<std::size_t N, std::size_t M>
class bitset2d : public std::bitset<N * M>
{
  public:
    typename std::bitset<N * M>::reference operator()(std::size_t n, std::size_t m)
    {
        return (*this)[n * M + m];
    }

    std::optional<std::tuple<size_t, size_t>> find_set(size_t start_from_n = 0,
                                                       size_t start_from_m = 0)
    {
        for (size_t pos = start_from_n * M + start_from_m; pos < N * M; pos++)
        {
            if ((*this)[pos])
            {
                return std::optional(std::tuple<size_t, size_t>(pos / M, pos % M));
            }
        }
        return std::nullopt;
    };
};

/**
 * Simple std::bitset<8> alternative that is 1 byte in size.
 */
class tiny_bitset8
{
  public:

    tiny_bitset8() = default;

    // NOLINTNEXTLINE(*-explicit-*)
    tiny_bitset8(uint8_t bits) : bits(bits) {}

    inline void set(size_t pos)
    {
        assert(pos < 8);
        bits |= (1 << pos);
    }

    inline void reset(size_t pos)
    {
        assert(pos < 8);
        bits &= ~(1 << pos);
    }

    [[nodiscard]] inline bool test(size_t pos) const
    {
        assert(pos < 8);
        return (bits & (1 << pos)) != 0;
    }

  private:
    uint8_t bits = 0;
};

} // namespace bitset
