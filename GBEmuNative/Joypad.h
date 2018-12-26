#pragma once

#include "IMemoryBusDevice.h"
#include "MemoryBus.h"

#include <memory>

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

		auto numJoysticks = SDL_NumJoysticks();

		// Search for the specific knockoff  USB NES pad I own, because it's awesome
		for (int i = 0; i < SDL_NumJoysticks(); ++i)
		{
			std::shared_ptr<SDL_Joystick> pJoystick(SDL_JoystickOpen(i), SDL_JoystickClose);
			std::string joystickName = SDL_JoystickName(pJoystick.get());
			if (joystickName == "USB Gamepad ") // note the space
			{
				m_pJoystick = pJoystick;
			}
		}

		//for (;;)
		//{
		//	SDL_JoystickUpdate();

		//	for (int i = 0; i < SDL_JoystickNumAxes(m_pJoystick.get()); ++i)
		//	{
		//		auto value = SDL_JoystickGetAxis(m_pJoystick.get(), i);
		//		printf("axis %d: %d\n", i, value);
		//	}

		//	for (int i = 0; i < SDL_JoystickNumHats(m_pJoystick.get()); ++i)
		//	{
		//		auto value = SDL_JoystickGetHat(m_pJoystick.get(), i);
		//		printf("hat %d: %d\n", i, value);
		//	}

		//	for (int i = 0; i < SDL_JoystickNumButtons(m_pJoystick.get()); ++i)
		//	{
		//		auto value = SDL_JoystickGetButton(m_pJoystick.get(), i);
		//		printf("button %d: %d\n", i, value);
		//	}
		//	Sleep(100);
		//}
	}

	void Reset()
	{
		m_updateTimeLeft = 0.0f;
		P1_JOYP = 0x0F;
		m_lastP1_JOYP = 0xFF;
	}

	void Update(float seconds)
	{
		m_updateTimeLeft += seconds;

		bool forceUpdate = false;
		if ((m_lastP1_JOYP & 0xF0) != (P1_JOYP & 0xF0))
		{
			forceUpdate = true;
		}
		m_lastP1_JOYP = P1_JOYP;

		while ((m_updateTimeLeft > 0.0f) || forceUpdate)
		{
			if (m_updateTimeLeft > 0.0f)
			{
				m_updateTimeLeft -= 1.0f / 60.0f;
			}
			forceUpdate = false;

			//SDL_JoystickUpdate();

			const auto pKeyState = SDL_GetKeyboardState(nullptr);

			Uint8 oldValues = P1_JOYP & 0x0F;
			
			if ((P1_JOYP & Bit5) == 0)
			{
				// Buttons
				SetBitValue(P1_JOYP, 0, pKeyState[SDL_SCANCODE_E] == 0); // A: P
				SetBitValue(P1_JOYP, 1, pKeyState[SDL_SCANCODE_R] == 0); // B: O
				SetBitValue(P1_JOYP, 2, pKeyState[SDL_SCANCODE_Q] == 0); // Select: Q
				SetBitValue(P1_JOYP, 3, pKeyState[SDL_SCANCODE_W] == 0); // Start: W

				if (m_pJoystick)
				{
					// A
					if (SDL_JoystickGetButton(m_pJoystick.get(), 1))
					{
						P1_JOYP &= ~Bit0;
					}

					// B
					if (SDL_JoystickGetButton(m_pJoystick.get(), 2))
					{
						P1_JOYP &= ~Bit1;
					}

					// Select
					if (SDL_JoystickGetButton(m_pJoystick.get(), 8))
					{
						P1_JOYP &= ~Bit2;
					}

					// Start
					if (SDL_JoystickGetButton(m_pJoystick.get(), 9))
					{
						P1_JOYP &= ~Bit3;
					}
				}
			}
			
			if ((P1_JOYP & Bit4) == 0)
			{
				// D-pad
				SetBitValue(P1_JOYP, 0, pKeyState[SDL_SCANCODE_RIGHT] == 0);
				SetBitValue(P1_JOYP, 1, pKeyState[SDL_SCANCODE_LEFT] == 0);
				SetBitValue(P1_JOYP, 2, pKeyState[SDL_SCANCODE_UP] == 0);
				SetBitValue(P1_JOYP, 3, pKeyState[SDL_SCANCODE_DOWN] == 0);
				
				if (m_pJoystick)
				{
					// Right/left
					auto axis0 = SDL_JoystickGetAxis(m_pJoystick.get(), 0);
					if (axis0 > 16384)
					{
						P1_JOYP &= ~Bit0;
					}
					else if (axis0 < -16384)
					{
						P1_JOYP &= ~Bit1;
					}

					// Up/down
					auto axis4 = SDL_JoystickGetAxis(m_pJoystick.get(), 4);
					if (axis4 < -16384)
					{
						P1_JOYP &= ~Bit2;
					}
					else if (axis4 > 16384)
					{
						P1_JOYP &= ~Bit3;
					}
				}
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
	Uint8 m_lastP1_JOYP;
	std::shared_ptr<SDL_Joystick> m_pJoystick;

	std::shared_ptr<MemoryBus> m_pMemory;
	std::shared_ptr<Cpu> m_pCpu;
};