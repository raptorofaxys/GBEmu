#pragma once

#include "Rom.h"
#include "Memory.h"
#include "Cpu.h"

class GameBoy
{
public:
	static const int kScreenWidth = 160;
	static const int kScreenHeight = 144;

	GameBoy(const char* pFileName)
	{
		m_pRom.reset(new Rom(pFileName));
		m_pMemory.reset(new Memory(m_pRom));
		m_pCpu.reset(new Cpu(m_pMemory));
	}

	const Rom& GetRom() const
	{
		return *m_pRom;
	}

	void Update(float seconds)
	{
		m_pCpu->Update(seconds);
	}

private:
	std::shared_ptr<Rom> m_pRom;
	std::shared_ptr<Memory> m_pMemory;
	std::shared_ptr<Cpu> m_pCpu;
};
