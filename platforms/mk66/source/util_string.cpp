#include "util_string.h"
#include <cctype>
#include <cstdlib>
#include <cstring>

bool compareStrings(const char* a, const char* b)
{
    size_t aLength = std::strlen(a);
    size_t bLength = std::strlen(b);

    while ((*a != 0) && (*b != 0))
    {
        bool isDigitA = std::isdigit(*a);
        bool isDigitB = std::isdigit(*b);

        if (isDigitA && isDigitB)
        {
            int intA = std::atoi(a);
            int intB = std::atoi(b);

            if (intA == intB)
            {
                while (std::isdigit(*a))
                {
                    a++;
                    b++;
                }
            }
            else
            {
                return intA < intB;
            }
        }
        else if (isDigitA != isDigitB)
        {
            return isDigitA;
        }
        else
        {
            char tempA = std::tolower(*a);
            char tempB = std::tolower(*b);

            if (tempA == tempB)
            {
                a++;
                b++;
            }
            else
            {
                return tempA < tempB;
            }
        }
    }

    return aLength <= bLength;
}

void polystrncpy(char* dest, const char* src, int destSize)
{
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = 0;
}
