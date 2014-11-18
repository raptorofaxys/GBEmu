#include "MemoryBus.h"

bool MemoryBus::breakOnRegisterAccess = true;
Uint16 MemoryBus::breakRegister = 0;//static_cast<Uint16>(MemoryMappedRegisters::IF);