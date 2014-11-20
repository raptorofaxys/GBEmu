#pragma once

#include "IMemoryBusDevice.h"

#include "Utils.h"

class GameLinkPort : public IMemoryBusDevice
{
public:
	enum class Registers
	{
		SB = 0xFF01,	// Serial transfer data
		SC = 0xFF02,	// Serial transfer control
	};

	GameLinkPort()
	{
		Reset();
	}

	void Reset()
	{
		SB = 0;
		SC = 0;
	}

	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value)
	{
		switch (address)
		{
		case Registers::SC:
			{
				if (requestType == MemoryRequestType::Read)
				{
					value = SC;
				}
				else
				{
					SC = value;
					
					//@TODO: timing emulation, internal/external clock
					if (value & Bit7)
					{
						//printf("Serial output byte: %c (0x%02lX)\n", m_SB, m_SB);
						printf("%c", SB);
						SC &= ~Bit7;
						// Read zeroes
						SB = 0;
					}
				}
				return true;
			}

		SERVICE_MMR_RW(SB)
		}
	
		return false;
	}
private:
	Uint8 SB;
	Uint8 SC;
};