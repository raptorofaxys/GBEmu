#pragma once

#include "MemoryMapper.h"
#include "Utils.h"

class RomOnlyMapper : public MemoryMapper
{
public:
	RomOnlyMapper(const std::shared_ptr<Rom>& rom)
		: m_pRom(rom)
	{
	}

	virtual void Reset()
	{
	}

	static const int kRomBase = 0x0000;
	static const int kRomSize = 0x8000;

	static const int kRamBankBase = 0xA000;
	static const int kRamBankSize = 0xC000 - kRamBankBase;
	static const int kExternalRamSize = kRamBankSize * 4;

	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value)
	{
		if (IsAddressInRange(address, kRomBase, kRomSize))
		{
			if (requestType == MemoryRequestType::Write)
			{
				// Just ignore the write, it won't do anything
				return true;
			}
			else
			{
				value = m_pRom->GetRom()[address - kRomBase];
				return true;
			}
		}
		else if (ServiceMemoryRangeRequest(requestType, address, value, kRamBankBase, kRamBankSize, m_externalRam))
		{
			return true;
		}

		return false;
	}

private:
	std::shared_ptr<Rom> m_pRom;

	Uint8 m_externalRam[kExternalRamSize];
};