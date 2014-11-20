#pragma once

#include "Rom.h"
#include "MemoryBus.h"
#include "Cpu.h"
#include "Timer.h"
#include "GameLinkPort.h"
#include "Lcd.h"

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
		m_pMemory.reset(new MemoryBus());
		m_pCpu.reset(new Cpu(m_pMemory));
		m_pTimer.reset(new Timer(m_pMemory));
		m_pGameLinkPort.reset(new GameLinkPort());
		m_pLcd.reset(new Lcd());

		m_pMemory->AddDevice(m_pRom);
		m_pMemory->AddDevice(m_pTimer);
		m_pMemory->AddDevice(m_pGameLinkPort);
		m_pMemory->AddDevice(m_pLcd);

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
		//Write8(MemoryBus::MemoryMappedRegisters::TIMA, 0);
		//m_pMemory->TIMA = 0;
	}

	void Update(float seconds)
	{
		// CPU cycles are counted here, and not in the CPU, because they are the atom of emulator execution
		m_cyclesRemaining += seconds * Cpu::kCyclesPerSecond;
	
		float timePerClockCycle = 1.0f / Cpu::kCyclesPerSecond;

		while (m_cyclesRemaining > 0)
		{
			//if (m_totalCyclesExecuted > Cpu::kCyclesPerSecond)
			//{
			//	printf("%f cycles executed TIMA: %d\n", m_totalCyclesExecuted, m_pMemory->TIMA);
			//}

			m_pCpu->SetTraceEnabled(m_debuggerState == DebuggerState::SingleStepping);
			//m_pCpu->SetTraceEnabled(true);
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
			//auto instructionCycles = 4;
			m_totalCyclesExecuted += instructionCycles;
			g_totalCyclesExecuted += instructionCycles;
			m_cyclesRemaining -= instructionCycles;

			auto timeSpentOnInstruction = timePerClockCycle * instructionCycles;

			m_pTimer->Update(timeSpentOnInstruction);
			m_pLcd->Update(timeSpentOnInstruction);
		}
	}

private:
	std::shared_ptr<Rom> m_pRom;
	std::shared_ptr<MemoryBus> m_pMemory;
	std::shared_ptr<Cpu> m_pCpu;
	std::shared_ptr<Timer> m_pTimer;
	std::shared_ptr<GameLinkPort> m_pGameLinkPort;
	std::shared_ptr<Lcd> m_pLcd;

	float m_totalCyclesExecuted;
	float m_cyclesRemaining;
	DebuggerState m_debuggerState;
};