#include "Memory.h"

bool Memory::breakOnRegisterAccess = true;
Uint16 Memory::breakRegister = 0;//static_cast<Uint16>(MemoryMappedRegisters::IF);