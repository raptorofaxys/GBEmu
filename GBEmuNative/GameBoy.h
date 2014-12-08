#pragma once

#include "Rom.h"
#include "MemoryBus.h"
#include "Cpu.h"
#include "Timer.h"
#include "Joypad.h"
#include "GameLinkPort.h"
#include "Lcd.h"
#include "Sound.h"
#include "UnusableMemory.h"
#include "UnknownMemoryMappedRegisters.h"

#include "RomOnlyMapper.h"
#include "Mbc1Mapper.h"

class GameBoy
{
public:
	enum class DebuggerState
	{
		Running,
		SingleStepping
	};

	GameBoy(const char* pFileName, SDL_Renderer* pRenderer)
	{
		m_pFrameBuffer.reset(SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, Lcd::kScreenWidth, Lcd::kScreenHeight), SDL_DestroyTexture);
		if (!pRenderer)
		{
			throw Exception("Couldn't create framebuffer texture");
		}

		m_pRom.reset(new Rom(pFileName));

		auto cartridgeType = m_pRom->GetCartridgeType();
		switch (cartridgeType)
		{
		case CartridgeType::ROM_ONLY: m_pMapper.reset(new RomOnlyMapper(m_pRom)); break;
		case CartridgeType::MBC1: m_pMapper.reset(new Mbc1Mapper(m_pRom)); break;

		default:
			throw Exception("Unsupported cartridge type: %d", cartridgeType);
		}

		m_pMemory.reset(new MemoryBus());
		m_pCpu.reset(new Cpu(m_pMemory));
		m_pTimer.reset(new Timer(m_pMemory, m_pCpu));
		m_pJoypad.reset(new Joypad(m_pMemory, m_pCpu));
		m_pGameLinkPort.reset(new GameLinkPort());
		m_pLcd.reset(new Lcd(m_pMemory, m_pCpu, m_pFrameBuffer));
		m_pSound.reset(new Sound());
		m_pUnusableMemory.reset(new UnusableMemory());
		m_pUnknownMemoryMappedRegisters.reset(new UnknownMemoryMappedRegisters());

		m_pMemory->AddDevice(m_pMapper);
		m_pMemory->AddDevice(m_pCpu);
		m_pMemory->AddDevice(m_pTimer);
		m_pMemory->AddDevice(m_pJoypad);
		m_pMemory->AddDevice(m_pGameLinkPort);
		m_pMemory->AddDevice(m_pLcd);
		m_pMemory->AddDevice(m_pSound);
		m_pMemory->AddDevice(m_pUnusableMemory);
		m_pMemory->AddDevice(m_pUnknownMemoryMappedRegisters);

		Reset();
	}

	const Rom& GetRom() const
	{
		return *m_pRom;
	}

	SDL_Texture* GetFrameBufferTexture() const
	{
		return m_pFrameBuffer.get();
	}

	void Reset()
	{
		m_totalCyclesExecuted = 0.0f;
		m_cyclesRemaining = 0.0f;
		m_debuggerState = DebuggerState::Running;

		m_pMemory->Reset();
		m_pCpu->Reset();
		m_pTimer->Reset();
		m_pJoypad->Reset();
		m_pLcd->Reset();
		m_pSound->Reset();
		m_pMapper->Reset();

		//@TODO: initial state
		//Write8(MemoryBus::MemoryMappedRegisters::TIMA, 0);
		//m_pMemory->TIMA = 0;
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

	void Step()
	{
		m_debuggerState = DebuggerState::SingleStepping;
		m_cyclesRemaining = 1;
	}

	void Go()
	{
		m_debuggerState = DebuggerState::Running;
	}

	void Update(float seconds)
	{
		if (m_debuggerState == DebuggerState::SingleStepping)
		{
			// Only advance time when user wishes to do so
			seconds = 0.0f;
		}

		// CPU cycles are counted here, and not in the CPU, because they are the atom of emulator execution
		m_cyclesRemaining += seconds * Cpu::kCyclesPerSecond;
	
		const float timePerClockCycle = 1.0f / Cpu::kCyclesPerSecond;

		while (m_cyclesRemaining > 0)
		{
			auto instructionCycles = m_pCpu->ExecuteSingleInstruction();
			//auto instructionCycles = 4;
			m_totalCyclesExecuted += instructionCycles;
			g_totalCyclesExecuted += instructionCycles;
			m_cyclesRemaining -= instructionCycles;

			auto timeSpentOnInstruction = timePerClockCycle * instructionCycles;

			m_pTimer->Update(timeSpentOnInstruction);
			m_pLcd->Update(timeSpentOnInstruction);
			m_pSound->Update(timeSpentOnInstruction);

			static Sint32 breakpointAddress = -1;
			if (m_pCpu->GetPC() == breakpointAddress)
			{
				Step();
			}

			m_pCpu->SetTraceEnabled(m_debuggerState == DebuggerState::SingleStepping);
			//m_pCpu->SetTraceEnabled(true);
			m_pCpu->DebugNextOpcode();
		}

		//@TODO: synchronize updates to LCD controller vblanks to avoid tearing
	}

private:
	// @TODO: possibly refactor into some kind of system component collection?
	std::shared_ptr<Rom> m_pRom;
	std::shared_ptr<MemoryMapper> m_pMapper;
	std::shared_ptr<MemoryBus> m_pMemory;
	std::shared_ptr<Cpu> m_pCpu;
	std::shared_ptr<Timer> m_pTimer;
	std::shared_ptr<Joypad> m_pJoypad;
	std::shared_ptr<GameLinkPort> m_pGameLinkPort;
	std::shared_ptr<Lcd> m_pLcd;
	std::shared_ptr<Sound> m_pSound;
	std::shared_ptr<UnusableMemory> m_pUnusableMemory;
	std::shared_ptr<UnknownMemoryMappedRegisters> m_pUnknownMemoryMappedRegisters;

	float m_totalCyclesExecuted;
	float m_cyclesRemaining;
	DebuggerState m_debuggerState;

	std::shared_ptr<SDL_Texture> m_pFrameBuffer;
};
