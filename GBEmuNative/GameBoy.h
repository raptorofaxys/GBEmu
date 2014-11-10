#pragma once

#include "Rom.h"
#include "Memory.h"
#include "Cpu.h"
#include "Timer.h"

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
		m_pTimer.reset(new Timer(m_pMemory));

		Reset();
	}

	const Rom& GetRom() const
	{
		return *m_pRom;
	}

	void Reset()
	{
		m_totalCyclesExecuted = 0.0f;
		m_cyclesRemaining = 0.0f;
		m_debuggerState = DebuggerState::Running;

		m_pMemory->Reset();
		m_pCpu->Reset();

		//@TODO: initial state
		//Write8(Memory::MemoryMappedRegisters::TIMA, 0);
		//m_pMemory->TIMA = 0;
	}

	void Update(float seconds)
	{
		// CPU cycles are counted here, and not in the CPU, because they are the atom of emulator execution
		m_cyclesRemaining += seconds * Cpu::kCyclesPerSecond;
	
		float timePerClockCycle = 1.0f / Cpu::kCyclesPerSecond;

		while (m_cyclesRemaining > 0)
		{
			printf("%f cycles executed\n", m_totalCyclesExecuted);

			m_pCpu->SetTraceEnabled(m_debuggerState == DebuggerState::SingleStepping);
			m_pCpu->SetTraceEnabled(true);
			m_pCpu->DebugNextOpcode();

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
			m_totalCyclesExecuted += instructionCycles;
			m_cyclesRemaining -= instructionCycles;

			auto timeSpentOnInstruction = timePerClockCycle * instructionCycles;

			m_pTimer->Update(timeSpentOnInstruction);
		}
	}

private:
	std::shared_ptr<Rom> m_pRom;
	std::shared_ptr<Memory> m_pMemory;
	std::shared_ptr<Cpu> m_pCpu;
	std::shared_ptr<Timer> m_pTimer;

	float m_totalCyclesExecuted;
	float m_cyclesRemaining;
	DebuggerState m_debuggerState;
};
