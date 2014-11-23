#pragma once

#include "IMemoryBusDevice.h"
#include "MemoryBus.h"

class Joypad : public IMemoryBusDevice
{
public:
	enum class Registers
	{
		P1_JOYP = 0xFF00, // Joypad
	};

	Joypad(const std::shared_ptr<MemoryBus>& memory)
		: m_pMemory(memory)
		, IF(memory->IF) //@TODO: possibly replace with access to CPU (or whatever memory bus device winds up servicing IF requests)
	{
		Reset();
	}

	void Reset()
	{
		P1_JOYP = 0;
	}

	void Update(float seconds)
	{
		//@TODO: support input by updating lower four bits of P1_JOYP
		//@TODO: support input interrupt and/or wake-from-STOP
	}
	
	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value)
	{
		switch (address)
		{
		case Registers::P1_JOYP:
			{
				if (requestType == MemoryRequestType::Write)
				{
					P1_JOYP = (P1_JOYP & 0x0F) | (value & 0xF0);
				}
				else
				{
					value = P1_JOYP;
				}
				return true;
			}
			break;
		}
	
		return false;
	}

	Uint8 P1_JOYP;
	Uint8& IF;
private:
	std::shared_ptr<MemoryBus> m_pMemory;
};