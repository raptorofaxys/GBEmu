#include "MemoryBus.h"

bool MemoryBus::dataBreakpointActive = true;
Uint16 MemoryBus::dataBreakpointAddress = 0xFF41;//static_cast<Uint16>(MemoryMappedRegisters::IF);