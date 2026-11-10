// Tracker Cycles (plan 4a): replace libstdc++'s verbose terminate handler,
// which pulls in the C++ demangler (~15 KB of flash) only to print the
// name of an uncaught exception's type. Exceptions are off (-fno-exceptions);
// anything that still calls std::terminate just aborts.
#include <cstdlib>

namespace __gnu_cxx {
void __verbose_terminate_handler() { abort(); }
}  // namespace __gnu_cxx
