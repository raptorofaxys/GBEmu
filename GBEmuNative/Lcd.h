#pragma once

#include "IMemoryBusDevice.h"

#include "Utils.h"

class Lcd : public IMemoryBusDevice
{
public:
	enum class Registers
	{
		LCDC = 0xFF40,	// LCD Control
		STAT = 0xFF41,	// LCDC Status
		SCY = 0xFF42,	// Scroll Y
		SCX = 0xFF43,	// Scroll X
		LY = 0xFF44,	// LCDC Y-coordinate
		BGP = 0xFF47,	// BG palette data
		OBP0 = 0xFF48,	// Object palette 0 data
		OBP1 = 0xFF49,	// Object palette 1 data
	};

	enum class State
	{
		HBlank,
		//VBlank, // implicitly derived from the scanline
		ReadingOam,
		ReadingOamAndVram,
	};

	static const int kVramBase = 0x8000;
	static const int kVramSize = 0x2000;

	static const int kOamBase = 0xFE00;
	static const int kOamSize = 0xFE9F - kOamBase + 1;

	Lcd()
	{
		Reset();
	}

	void Reset()
	{
		m_updateTimeLeft = 0.0f;
		m_nextState = State::ReadingOam;
		m_scanLine = 0;

		memset(m_vram, 0xFD, sizeof(m_vram));
		memset(m_oam, 0xFD, sizeof(m_oam));

		LCDC = 0;
		STAT = 0;
		SCY = 0;
		SCX = 0;
		LY = 0;
		BGP = 0;
		OBP0 = 0;
		OBP1 = 0;
	}

	void Update(float seconds)
	{
		// Documentation on the exact timing here quotes various numbers.
		m_updateTimeLeft += seconds;

		while (m_updateTimeLeft > 0.0f)
		{
			int mode = 0;
			switch (m_nextState)
			{
			case State::ReadingOam:
				{
					++m_scanLine;
					++LY;

					if (m_scanLine > 153)
					{
						m_scanLine = 0;
						LY = 0;
					}

					m_updateTimeLeft -= 0.000019f;
					mode = 2;
					m_nextState = State::ReadingOamAndVram;
				}
				break;
			case State::ReadingOamAndVram:
				{
					m_updateTimeLeft -= 0.000041f;
					mode = 3;
					m_nextState = State::HBlank;
				}
				break;
			case State::HBlank:
				{
					m_updateTimeLeft -= 0.0000486f;
					mode = 0;
					m_nextState = State::ReadingOam;
				}
				break;
			}

			if (m_scanLine >= 144)
			{
				// Vblank
				mode = 1;
			}

			// Mode is the lower two bits of the STAT register
			STAT = (STAT & ~(Bit1 | Bit0)) | (mode);
		}
	}

	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value)
	{
		if (ServiceMemoryRangeRequest(requestType, address, value, kVramBase, kVramSize, m_vram))
		{
			return true;
		}
		else if (ServiceMemoryRangeRequest(requestType, address, value, kOamBase, kOamSize, m_oam))
		{
			return true;
		}
		else
		{
			switch (address)
			{
			SERVICE_MMR_RW(LCDC)

			case Registers::STAT:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = STAT;
					}
					else
					{
						// Bits 3-6 are read/write, bits 0-2 are read-only
						STAT = (value & (Bit6 | Bit5 | Bit4 | Bit3)) | (STAT & (Bit2 | Bit1 | Bit0));
						//@TODO: coincidence interrupt
						//@TODO: OAM interrupt
						//@TODO: vblank interrupt
						//@TODO: hblank interrupt
					}
					return true;
				}
		
			SERVICE_MMR_RW(SCY)
			SERVICE_MMR_RW(SCX)

			case Registers::LY:
				{
					if (requestType == MemoryRequestType::Write)
					{
						LY = 0;
					}
					else
					{
						value = LY;
					}
					return true;
				}

			SERVICE_MMR_RW(BGP)
			SERVICE_MMR_RW(OBP0)
			SERVICE_MMR_RW(OBP1)
			}
		}
	
		return false;
	}
private:
	float m_updateTimeLeft;
	State m_nextState;
	int m_scanLine;

	Uint8 m_vram[kVramSize];
	Uint8 m_oam[kOamSize];

	Uint8 LCDC;
	Uint8 STAT;
	Uint8 SCY;
	Uint8 SCX;
	Uint8 LY;
	Uint8 BGP;
	Uint8 OBP0;
	Uint8 OBP1;
};