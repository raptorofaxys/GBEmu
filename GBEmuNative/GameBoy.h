#pragma once

#include "Rom.h"
#include "Memory.h"
#include "Cpu.h"

class GameBoy
{
public:
	static const int kScreenWidth = 160;
	static const int kScreenHeight = 144;

	enum class DebuggerState
	{
		Running,
		SingleStepping
	};

	GameBoy(const char* pFileName)
	{
		m_pRom.reset(new Rom(pFileName));
		m_pMemory.reset(new Memory(m_pRom));
		m_pCpu.reset(new Cpu(m_pMemory));

		Reset();
	}

	const Rom& GetRom() const
	{
		return *m_pRom;
	}

	void Reset()
	{
		m_cyclesRemaining = 0.0f;
		m_debuggerState = DebuggerState::Running;

		m_pMemory->Reset();
		m_pCpu->Reset();
	}

	void Update(float seconds)
	{
		m_cyclesRemaining += seconds * Cpu::kCyclesPerSecond;
	
		while (m_cyclesRemaining > 0)
		{
			SDL_Keycode keycode = SDLK_UNKNOWN;
			if (m_debuggerState == DebuggerState::SingleStepping)
			{
				keycode = DebugWaitForKeypress();
			}
			else
			{
				keycode = DebugCheckForKeypress();
			}

			switch (keycode)
			{
			case SDLK_g: m_debuggerState = DebuggerState::Running; break;
			case SDLK_s: m_debuggerState = DebuggerState::SingleStepping; break;
			}

			auto instructionCycles = m_pCpu->ExecuteSingleInstruction();

			m_cyclesRemaining -= instructionCycles;
		}
	}

private:
	std::shared_ptr<Rom> m_pRom;
	std::shared_ptr<Memory> m_pMemory;
	std::shared_ptr<Cpu> m_pCpu;

	float m_cyclesRemaining;
	DebuggerState m_debuggerState;
};
