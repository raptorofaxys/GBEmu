#pragma once

#include "IMemoryBusDevice.h"

#include "Utils.h"

#include "SDL.h"

#include <string>
#include <vector>

class Rom : public IMemoryBusDevice // @TODO: hide behind memory mapper IMemoryBusDevice
{
public:
	static const int kRomBase = 0x0000;
	static const int kRomSize = 0x8000;

	Rom(const char* pFileName)
	{
		LoadFromFile(pFileName);
	}

	std::string GetRomName() const
	{
		std::string result;

		const Uint8* p = &m_pRom[kNameOffset];
		for (int i = 0; i < kNameLength; ++i)
		{
			if (*p)
			{
				result += *p;
			}
			++p;
		}

		return result;
	}

	const std::vector<Uint8>& GetRom()
	{
		return m_pRom;
	}

	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value)
	{
		if (IsAddressInRange(address, kRomBase, kRomSize))
		{
			if (requestType == MemoryRequestType::Write)
			{
				throw Exception("Attempted to write to ROM area.");
			}
			else
			{
				value = m_pRom[address - kRomBase];
				return true;
			}
		}

		return false;
	}

private:
	static const int kNameOffset = 0x134;
	static const int kNameLength = 0x11;

	void LoadFromFile(const char* pFileName)
	{
		LoadFileAsByteArray(m_pRom, pFileName);
	}

	std::vector<Uint8> m_pRom;
};
