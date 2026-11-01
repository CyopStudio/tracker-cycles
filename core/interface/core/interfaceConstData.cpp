#include "interfaceDefs.h"

constexpr uint8_t InterfaceGlobals::fxIDs[FX_COUNT];
constexpr char InterfaceGlobals::rndFxName[FX_NAME_SIZE + 4];
constexpr char InterfaceGlobals::fxNames[FX_COUNT][FX_NAME_SIZE + 4];
constexpr char InterfaceGlobals::fxShortNames[FX_COUNT][FX_NAME_SIZE + 4];
constexpr const char* InterfaceGlobals::ptrAllFxNames[FX_COUNT + 1];
constexpr const char* InterfaceGlobals::ptrAllFxNamesShort[FX_COUNT + 1];
constexpr const char* const* InterfaceGlobals::ptrFxNames;
