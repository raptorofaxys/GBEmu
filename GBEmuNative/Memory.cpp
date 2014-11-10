#include "Memory.h"

bool Memory::breakOnRegisterAccess = true;
Uint16 Memory::breakRegister = static_cast<Uint16>(MemoryMappedRegisters::IF);