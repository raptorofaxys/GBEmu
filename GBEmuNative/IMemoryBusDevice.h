#pragma once

#include "SDL.h"

enum class MemoryRequestType
{
	Read,
	Write
};

class IMemoryBusDevice
{
public:
	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value) = 0;
};

#define SERVICE_MMR_RW(x) case Registers::x: { if (requestType == MemoryRequestType::Read) { value = x; } else { x = value; } return true; } break;
