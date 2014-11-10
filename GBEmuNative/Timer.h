#pragma once

#include "Memory.h"

class Timer
{
public:
	static int const kDivFrequency = 16384;

	Timer(const std::shared_ptr<Memory>& memory)
		: m_pMemory(memory)
		, DIV(memory->DIV)
		, TIMA(memory->TIMA)
		, TMA(memory->TMA)
		, TAC(memory->TAC)
		, IF(memory->IF)
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
	
	Uint8& DIV;
	Uint8& TIMA;
	Uint8& TMA;
	Uint8& TAC;
	Uint8& IF;
private:
	std::shared_ptr<Memory> m_pMemory;
	float m_DivTicksRemaining;
	float m_TimaTicksRemaining;
};