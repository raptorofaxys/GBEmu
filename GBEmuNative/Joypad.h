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

	Joypad(const std::shared_ptr<MemoryBus>& memory, const std::shared_ptr<Cpu>& cpu)
		: m_pMemory(memory)
		, m_pCpu(cpu)
	{
		Reset();
	}

	void Reset()
	{
		m_updateTimeLeft = 0.0f;
		P1_JOYP = 0x0F;
	}

	void Update(float seconds)
	{
		m_updateTimeLeft += seconds;
		while (m_updateTimeLeft > 0.0f)
		{
			m_updateTimeLeft -= 1.0f / 60.0f;

			const auto pKeyState = SDL_GetKeyboardState(nullptr);

			Uint8 oldValues = P1_JOYP & 0x0F;
			if (P1_JOYP & Bit5)
			{
				// Buttons
				SetBitValue(P1_JOYP, 0, pKeyState[SDL_SCANCODE_P] == 0); // A: P
				SetBitValue(P1_JOYP, 1, pKeyState[SDL_SCANCODE_O] == 0); // B: P
				SetBitValue(P1_JOYP, 2, pKeyState[SDL_SCANCODE_Q] == 0); // Select: Q
				SetBitValue(P1_JOYP, 3, pKeyState[SDL_SCANCODE_W] == 0); // Start: W
			}
			else
			{
				// D-pad
				SetBitValue(P1_JOYP, 0, pKeyState[SDL_SCANCODE_RIGHT] == 0);
				SetBitValue(P1_JOYP, 1, pKeyState[SDL_SCANCODE_LEFT] == 0);
				SetBitValue(P1_JOYP, 2, pKeyState[SDL_SCANCODE_UP] == 0);
				SetBitValue(P1_JOYP, 3, pKeyState[SDL_SCANCODE_DOWN] == 0);
			}
			Uint8 newValues = P1_JOYP & 0x0F;

			// If any input lines went low, fire an interrupt
			if ((oldValues ^ newValues) & ~newValues)
			{
				m_pCpu->SignalInterrupt(Bit4);
			}
		}
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
private:
	float m_updateTimeLeft;

	std::shared_ptr<MemoryBus> m_pMemory;
	std::shared_ptr<Cpu> m_pCpu;
};