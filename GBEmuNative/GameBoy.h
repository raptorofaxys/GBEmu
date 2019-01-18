#pragma once

#include "Rom.h"
#include "MemoryBus.h"
#include "Cpu.h"
#include "Timer.h"
#include "Joypad.h"
#include "GameLinkPort.h"
#include "Lcd.h"
#include "Sound.h"
#include "Memory.h"
#include "UnknownMemoryMappedRegisters.h"

#include "RomOnlyMapper.h"
#include "Mbc1Mapper.h"

#include "Analyzer.h"

class GameBoy
{
public:
	enum class DebuggerState
	{
		Running,
		SingleStepping
	};
	
	enum class TracingState
	{
		Disabled,
		SingleSteppingOnly,
		Enabled
	};

	GameBoy(const char* pFileName, SDL_Renderer* pRenderer)
	{
		m_pRom.reset(new Rom(pFileName));

		auto cartridgeType = m_pRom->GetCartridgeType();
		switch (cartridgeType)
		{
		case CartridgeType::ROM_ONLY: m_pMapper.reset(new RomOnlyMapper(m_pRom)); break;
		
		case CartridgeType::MBC1:
		case CartridgeType::MBC1_RAM:
		case CartridgeType::MBC1_RAM_BATTERY:
			m_pMapper.reset(new Mbc1Mapper(m_pRom)); break;

		default:
			throw Exception("Unsupported cartridge type: %d", cartridgeType);
		}

		m_pMemoryBus.reset(new MemoryBus());
		m_pMemory.reset(new Memory());
		m_pCpu.reset(new Cpu(m_pMemoryBus));
		m_pTimer.reset(new Timer(m_pCpu));
		m_pJoypad.reset(new Joypad(m_pCpu));
		m_pGameLinkPort.reset(new GameLinkPort(m_pCpu));
		m_pLcd.reset(new Lcd(m_pMemoryBus, m_pCpu, pRenderer));
		m_pSound.reset(new Sound());
		m_pUnknownMemoryMappedRegisters.reset(new UnknownMemoryMappedRegisters());

		m_pMemoryBus->AddDevice(m_pMemory);
		m_pMemoryBus->AddDevice(m_pMapper);
		m_pMemoryBus->AddDevice(m_pCpu);
		m_pMemoryBus->AddDevice(m_pTimer);
		m_pMemoryBus->AddDevice(m_pJoypad);
		m_pMemoryBus->AddDevice(m_pGameLinkPort);
		m_pMemoryBus->AddDevice(m_pLcd);
		m_pMemoryBus->AddDevice(m_pSound);
		m_pMemoryBus->AddDevice(m_pUnknownMemoryMappedRegisters);

		m_pAnalyzer.reset(new Analyzer(m_pMapper.get(), m_pCpu.get(), m_pMemoryBus.get()));

		m_pMemoryBus->LockDevices(m_pAnalyzer.get());

		m_tracingState = TracingState::Enabled;

		SetAnalyzerTracingState();
		m_pAnalyzer->OnStart(m_pRom->GetRomName().c_str());

		Reset();
	}

	const Rom& GetRom() const
	{
		return *m_pRom;
	}

	SDL_Texture* GetFrontFrameBufferTexture() const
	{
		return m_pLcd->GetFrontFrameBufferTexture();
	}

	void Reset()
	{
		m_totalCyclesExecuted = 0.0f;
		m_cyclesRemaining = 0.0f;
		m_debuggerState = DebuggerState::Running;
		//m_debuggerState = DebuggerState::SingleStepping;
		m_breakpointAddress = -1;
		m_lastUpdateAddress = -1;

		m_pMemoryBus->Reset();
		m_pMemory->Reset();
		m_pCpu->Reset();
		m_pTimer->Reset();
		m_pJoypad->Reset();
		m_pLcd->Reset();
		m_pSound->Reset();
		m_pMapper->Reset();
	}

	void ToggleStepping()
	{
		if (m_debuggerState == DebuggerState::SingleStepping)
		{
			m_debuggerState = DebuggerState::Running;
		}
		else
		{
			m_debuggerState = DebuggerState::SingleStepping;
		}
	}

	void Stop()
	{
		m_debuggerState = DebuggerState::SingleStepping;
		m_cyclesRemaining = 0;
	}

	void Step()
	{
		m_debuggerState = DebuggerState::SingleStepping;
		m_cyclesRemaining = 1;
	}

	void Go()
	{
		m_debuggerState = DebuggerState::Running;
	}
	
	void BreakAtNextInstruction()
	{
		auto PC = m_pCpu->GetPC();
		m_breakpointAddress = PC + m_pCpu->GetInstructionSize(PC);
		Go();
	}

	void BreakInDebugger()
	{
		DebugBreak();
	}

	void SetAnalyzerTracingState()
	{
		switch (m_tracingState)
		{
		case TracingState::Enabled: m_pAnalyzer->SetTracingEnabled(true); break;
		case TracingState::SingleSteppingOnly: m_pAnalyzer->SetTracingEnabled(m_debuggerState == DebuggerState::SingleStepping); break;
		case TracingState::Disabled: m_pAnalyzer->SetTracingEnabled(false); break;
		}
	}

	void Update(float seconds)
	{
		if (m_debuggerState == DebuggerState::SingleStepping)
		{
			// Only advance time when user wishes to do so
			seconds = 0.0f;
		}

		// CPU cycles are counted here, and not in the CPU, because they are the atom of emulator execution
		m_cyclesRemaining += seconds * MemoryBus::kCyclesPerSecond;
	
		for (;;)
		{
			if (m_pCpu->GetPC() != m_lastUpdateAddress)
			{
				if ((m_pCpu->GetPC() == m_breakpointAddress) || s_stopOnNextInstruction)
				{
					Stop();
					m_breakpointAddress = -1;
					s_stopOnNextInstruction = false;
				}

				SetAnalyzerTracingState();

				if (m_debuggerState == DebuggerState::SingleStepping)
				{
					m_pAnalyzer->FlushTrace();
				}
				
				m_lastUpdateAddress = m_pCpu->GetPC();
			}

			if (m_cyclesRemaining > 0)
			{
				auto instructionCycles = m_pCpu->ExecuteSingleInstruction();
				m_totalCyclesExecuted += instructionCycles;
				g_totalCyclesExecuted += instructionCycles;
				m_cyclesRemaining -= instructionCycles;

                const float secondsPerClockCycle = 1.0f / MemoryBus::kCyclesPerSecond;
                auto secondsSpentOnInstruction = secondsPerClockCycle * instructionCycles;

				m_pTimer->Update(secondsSpentOnInstruction);
				m_pJoypad->Update(secondsSpentOnInstruction);
				m_pLcd->Update(secondsSpentOnInstruction);
				m_pSound->Update(secondsSpentOnInstruction);
                m_pGameLinkPort->Update(secondsSpentOnInstruction);
			}
			else
			{
				break;
			}
		}
	}

private:
	static bool s_stopOnNextInstruction;
	
	// @TODO: possibly refactor into some kind of system component collection?
	std::shared_ptr<Analyzer> m_pAnalyzer;
	std::shared_ptr<Rom> m_pRom;
	std::shared_ptr<MemoryMapper> m_pMapper;
	std::shared_ptr<MemoryBus> m_pMemoryBus;
	std::shared_ptr<Memory> m_pMemory;
	std::shared_ptr<Cpu> m_pCpu;
	std::shared_ptr<Timer> m_pTimer;
	std::shared_ptr<Joypad> m_pJoypad;
	std::shared_ptr<GameLinkPort> m_pGameLinkPort;
	std::shared_ptr<Lcd> m_pLcd;
	std::shared_ptr<Sound> m_pSound;
	std::shared_ptr<UnknownMemoryMappedRegisters> m_pUnknownMemoryMappedRegisters;

	float m_totalCyclesExecuted;
	float m_cyclesRemaining;
	DebuggerState m_debuggerState;
	TracingState m_tracingState;
	Sint32 m_breakpointAddress;
	Sint32 m_lastUpdateAddress;
};
