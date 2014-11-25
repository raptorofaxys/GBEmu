#pragma once

#include "SDL.h"

#include "Utils.h"

enum class MemoryRequestType
{
	Read,
	Write
};

class IMemoryBusDevice
{
public:
	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value) = 0;
protected:
	bool ServiceMemoryRangeRequest(MemoryRequestType requestType, Uint16 address, Uint8& value, Uint16 rangeBase, Uint16 rangeSize, Uint8* pRangeMemory)
	{
		if (IsAddressInRange(address, rangeBase, rangeSize))
		{
			if (requestType == MemoryRequestType::Read)
			{
				value = pRangeMemory[address - rangeBase];
			}
			else
			{
				pRangeMemory[address - rangeBase] = value;
			}
			return true;
		}
		return false;
	}
};

#define SERVICE_MMR_RW(x) case Registers::x: { if (requestType == MemoryRequestType::Read) { value = x; } else { x = value; } return true; } break;
