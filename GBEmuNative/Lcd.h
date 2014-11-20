#pragma once

#include "IMemoryBusDevice.h"

#include "Utils.h"

class Lcd : public IMemoryBusDevice
{
public:
	enum class Registers
	{
		STAT = 0xFF41,	// LCDC Status
		LY = 0xFF44,	// LCDC Y-coordinate
	};

	enum class State
	{
		HBlank,
		//VBlank, // implicitly derived from the scanline
		ReadingOam,
		ReadingOamAndVram,
	};

	Lcd()
	{
		Reset();
	}

	void Reset()
	{
		m_updateTimeLeft = 0.0f;
		m_nextState = State::ReadingOam;
		m_scanLine = 0;
		STAT = 0;
		LY = 0;
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
		switch (address)
		{
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
			break;
		}
	
		return false;
	}
private:
	float m_updateTimeLeft;
	State m_nextState;
	int m_scanLine;
	Uint8 STAT;
	Uint8 LY;
};