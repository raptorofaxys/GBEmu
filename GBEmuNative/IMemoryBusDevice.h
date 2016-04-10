#pragma once

#include "SDL.h"

#include "Utils.h"

enum class MemoryRequestType
{
	Read,
	Write
};

class Analyzer;

class IMemoryBusDevice
{
public:
	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value) = 0;
	void SetAnalyzer(Analyzer* pAnalyzer) { m_pAnalyzer = pAnalyzer;  } //@LAME
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
	Analyzer* GetAnalyzer() const { SDL_assert(m_pAnalyzer != nullptr); return m_pAnalyzer; }
private:
	Analyzer* m_pAnalyzer = nullptr;
};

#define SERVICE_MMR_RW(x) case Registers::x: { if (requestType == MemoryRequestType::Read) { value = x; } else { x = value; } return true; } break;
