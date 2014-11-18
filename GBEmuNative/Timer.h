#pragma once

#include "IMemoryBusDevice.h"

class Timer : public IMemoryBusDevice
{
public:
	enum class Registers
	{
		DIV = 0xFF04,	// Divider register
		TIMA = 0xFF05,	// Timer counter
		TMA = 0xFF06,	// Timer modulo
		TAC = 0xFF07,	// Timer control
	};

	static int const kDivFrequency = 16384;

	Timer(const std::shared_ptr<MemoryBus>& memory)
		: m_pMemory(memory)
		, IF(memory->IF) //@TODO: possibly replace with access to CPU (or whatever memory bus device winds up servicing IF requests)
	{
		Reset();
	}

	void Reset()
	{
		m_DivTicksRemaining = 0.0f;
		m_TimaTicksRemaining = 0.0f;

		DIV = 0;
		TIMA = 0;
		TMA = 0;
		TAC = 0;
	}

	void Update(float seconds)
	{
		m_DivTicksRemaining += seconds * kDivFrequency;
		while (m_DivTicksRemaining > 1.0f)
		{
			++DIV;
			m_DivTicksRemaining -= 1.0f;
		}

		// If the timer is enabled
		if (TAC & Bit2)
		{
			int frequency = 0;
			switch (TAC & 0x3)
			{
			case 0: frequency = 4096; break;
			case 1: frequency = 262144; break;
			case 2: frequency = 65536; break;
			case 3: frequency = 16384; break;
			}
			m_TimaTicksRemaining += seconds * frequency;

			while (m_TimaTicksRemaining > 1.0f)
			{
				if (TIMA == 0xFF)
				{
					TIMA = TMA;
					IF |= Bit2; // Request timer interrupt
				}
				++TIMA;
				m_TimaTicksRemaining -= 1.0f;
			}
		}
	}
	
	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value)
	{
		switch (address)
		{
		case Registers::DIV:
			{
				if (requestType == MemoryRequestType::Write)
				{
					DIV = 0;
				}
				else
				{
					value = DIV;
				}
				return true;
			}
			break;
		SERVICE_MMR_RW(TIMA)
		SERVICE_MMR_RW(TMA)
		SERVICE_MMR_RW(TAC)
		}
	
		return false;
	}

	Uint8 DIV;
	Uint8 TIMA;
	Uint8 TMA;
	Uint8 TAC;
	Uint8& IF;
private:
	std::shared_ptr<MemoryBus> m_pMemory;
	float m_DivTicksRemaining;
	float m_TimaTicksRemaining;
};