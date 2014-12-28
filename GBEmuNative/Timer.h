#pragma once

#include "IMemoryBusDevice.h"
#include "Cpu.h"

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

	Timer(const std::shared_ptr<MemoryBus>& memory, const std::shared_ptr<Cpu>& cpu)
		: m_pMemory(memory)
		, m_pCpu(cpu)
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
				// Handle overflow
				if (TIMA == 0xFF)
				{
					TIMA = TMA;
					m_pCpu->SignalInterrupt(Bit2);
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
private:
	std::shared_ptr<MemoryBus> m_pMemory;
	std::shared_ptr<Cpu> m_pCpu;
	float m_DivTicksRemaining;
	float m_TimaTicksRemaining;
};