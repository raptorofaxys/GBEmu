#pragma once

#include "IMemoryBusDevice.h"

#include "Utils.h"

#include "SDL.h"

#include <string>
#include <vector>

enum class CartridgeType
{
	ROM_ONLY = 0x00,
	MBC1 = 0x01,
	MBC1_RAM = 0x02,
	MBC1_RAM_BATTERY = 0x03,
	MBC2 = 0x05,
	MBC2_BATTERY = 0x06,
	ROM_RAM = 0x08,
	ROM_RAM_BATTERY = 0x09,
	MMM01 = 0x0B,
	MMM01_RAM = 0x0C,
	MMM01_RAM_BATTERY = 0x0D,
	MBC3_TIMER_BATTERY = 0x0F,
	MBC3_TIMER_RAM_BATTERY = 0x10,
	MBC3 = 0x11,
	MBC3_RAM = 0x12,
	MBC3_RAM_BATTERY = 0x13,
	MBC4 = 0x15,
	MBC4_RAM = 0x16,
	MBC4_RAM_BATTERY = 0x17,
	MBC5 = 0x19,
	MBC5_RAM = 0x1A,
	MBC5_RAM_BATTERY = 0x1B,
	MBC5_RUMBLE = 0x1C,
	MBC5_RUMBLE_RAM = 0x1D,
	MBC5_RUMBLE_RAM_BATTERY = 0x1E,
	POCKET_CAMERA = 0xFC,
	BANDAI_TAMA5 = 0xFD,
	HuC3 = 0xFE,
	HuC1_RAM_BATTERY = 0xFF,
};

class Rom
{
public:
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

	CartridgeType GetCartridgeType() const
	{
		return static_cast<CartridgeType>(m_pRom[kCartridgeTypeOffset]);
	}

	const std::vector<Uint8>& GetRom()
	{
		return m_pRom;
	}

private:
	static const int kNameOffset = 0x134;
	static const int kNameLength = 0x11;
	static const int kCartridgeTypeOffset = 0x147;

	void LoadFromFile(const char* pFileName)
	{
		LoadFileAsByteArray(m_pRom, pFileName);
	}

	std::vector<Uint8> m_pRom;
};
