#pragma once

#include "Rom.h"

#include <memory>

#include "SDL.h"

//class Mapper

extern float g_totalCyclesExecuted;

#define MMR_NAME(x) x

// Define memory addresses for all the memory-mapped registers
enum class MemoryMappedRegisters
{
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) name = addx,
#define DEFINE_MEMORY_MAPPED_REGISTER_R(addx, name) name = addx,
#define DEFINE_MEMORY_MAPPED_REGISTER_W(addx, name) name = addx,
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW
#undef DEFINE_MEMORY_MAPPED_REGISTER_R
#undef DEFINE_MEMORY_MAPPED_REGISTER_W
};

class Memory
{
public:
	enum class OperationMode
	{
		RomOnly = 0x00,
		MBC1 = 0x01,
	};

	// RomOnly
	static const int kRomOnlyBase = 0x0000;
	static const int kRomOnlySize = 0x8000;

	// MBC1
	static const int kRomBank00Base = 0x0000;
	static const int kRomBank00Size = 0x4000;
	static const int kRomBankNBase = 0x4000;
	static const int kRomBankNSize = 0x4000;

	static const int kVramBase = 0x8000;
	static const int kVramSize = 0x2000;

	static const int kWorkMemoryBase = 0xC000;
	static const int kWorkMemorySize = 0x2000; // 8k (not supporting CGB switchable mode)

	// Handling this specially because it overlays memory-mapped registers, but it's just the same as working memory
	static const int kEchoBase = 0xE000;
	static const int kEchoSize = 0xFE00 - kEchoBase;

	static const int kHramMemoryBase = 0xFF80;
	static const int kHramMemorySize = 0xFFFF - kHramMemoryBase; // last byte is IE register

	Memory(const std::shared_ptr<Rom>& rom)
		: m_pRom(rom)
	{
		m_mode = static_cast<OperationMode>(rom->GetRom()[0x147]);
		SDL_assert((m_mode == OperationMode::RomOnly) || (m_mode == OperationMode::MBC1));
		Reset();
	}

	void Reset()
	{
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) MMR_NAME(name) = 0x00;
#define DEFINE_MEMORY_MAPPED_REGISTER_R(addx, name) MMR_NAME(name) = 0x00;
#define DEFINE_MEMORY_MAPPED_REGISTER_W(addx, name) MMR_NAME(name) = 0x00;
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW
#undef DEFINE_MEMORY_MAPPED_REGISTER_R
#undef DEFINE_MEMORY_MAPPED_REGISTER_W
	}

	Uint8 Read8(Uint16 address, bool throwIfFailed = true, bool* pSuccess = nullptr)
	{
		if (pSuccess)
		{
			*pSuccess = true;
		}

		if (IsAddressInRange(address, kRomOnlyBase, kRomOnlySize))
		{
			return m_pRom->GetRom()[address];
		}
		else if (IsAddressInRange(address, kVramBase, kVramSize))
		{
			return m_vram[address - kVramBase];
		}
		else if (IsAddressInRange(address, kWorkMemoryBase, kWorkMemorySize))
		{
			return m_workMemory[address - kWorkMemoryBase];
		}
		else if (IsAddressInRange(address, kEchoBase, kEchoSize))
		{
			return m_workMemory[address - kEchoBase];
		}
		else if (IsAddressInRange(address, kHramMemoryBase, kHramMemorySize))
		{
			return m_hram[address - kHramMemoryBase];
		}
		else
		{
			return ReadMemoryMappedRegister(address, throwIfFailed, pSuccess);
		}
	}
	
	bool SafeRead8(Uint16 address, Uint8& value)
	{
		bool success = true;
		value = Read8(address, false, &success);
		return success;
	}

	Uint16 Read16(Uint16 address)
	{
		// Must handle as two reads, because the address can cross range boundaries
		return Make16(Read8(address + 1), Read8(address));
	}

	void Write8(Uint16 address, Uint8 value)
	{
		if (IsAddressInRange(address, kRomOnlyBase, kRomOnlySize))
		{
			throw Exception("Attempted to write to ROM area.");
		}
		else if (IsAddressInRange(address, kVramBase, kVramSize))
		{
			m_vram[address - kVramBase] = value;
		}
		else if (IsAddressInRange(address, kWorkMemoryBase, kWorkMemorySize))
		{
			m_workMemory[address - kWorkMemoryBase] = value;
		}
		else if (IsAddressInRange(address, kEchoBase, kEchoSize))
		{
			m_workMemory[address - kEchoBase] = value;
		}
		else if (IsAddressInRange(address, kHramMemoryBase, kHramMemorySize))
		{
			m_hram[address - kHramMemoryBase] = value;
		}
		else
		{
			WriteMemoryMappedRegister(address, value);
		}
	}

	void Write16(Uint16 address, Uint16 value)
	{
		Write8(address, GetLow8(value));
		Write8(address + 1, GetHigh8(value));
	}

	// Declare storage for all the memory-mapped registers
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) Uint8 MMR_NAME(name);
#define DEFINE_MEMORY_MAPPED_REGISTER_R(addx, name) Uint8 MMR_NAME(name);
#define DEFINE_MEMORY_MAPPED_REGISTER_W(addx, name) Uint8 MMR_NAME(name);
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW
#undef DEFINE_MEMORY_MAPPED_REGISTER_R
#undef DEFINE_MEMORY_MAPPED_REGISTER_W

private:

	bool IsAddressInRange(Uint16 address, Uint16 base, Uint16 rangeSize)
	{
		return (address >= base) && (address < base + rangeSize);
	}

	static bool breakOnRegisterAccess;
	static Uint16 breakRegister;

	Uint8 ReadMemoryMappedRegister(Uint16 address, bool throwIfFailed = true, bool* pSuccess = nullptr)
	{
		breakOnRegisterAccess = true;
		breakRegister = static_cast<Uint16>(MemoryMappedRegisters::IF);

		if (breakOnRegisterAccess && (address == breakRegister))
		{
			printf("Read 0x%04lX @ %f c (TIMA = %d)\n", address, g_totalCyclesExecuted, TIMA);
			int x = 3;
		}

		if (pSuccess)
		{
			*pSuccess = true;
		}
			
		switch (address)
		{
			// Handle reads to all the memory-mapped registers
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) case MemoryMappedRegisters::name: return MMR_NAME(name);
#define DEFINE_MEMORY_MAPPED_REGISTER_R(addx, name) case MemoryMappedRegisters::name: return MMR_NAME(name);
#define DEFINE_MEMORY_MAPPED_REGISTER_W(addx, name) case MemoryMappedRegisters::name: if (throwIfFailed) { throw Exception("Read from write-only register"); } if (pSuccess) { *pSuccess = false; } return 0xFF;
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW
#undef DEFINE_MEMORY_MAPPED_REGISTER_R
#undef DEFINE_MEMORY_MAPPED_REGISTER_W
		default:
			if (throwIfFailed)
			{
				throw NotImplementedException();
			}
			if (pSuccess)
			{
				*pSuccess = false;
			}
			return 0xFF;
		}
	}

	void WriteMemoryMappedRegister(Uint16 address, Uint8 value)
	{
		if (breakOnRegisterAccess && (address == breakRegister))
		{
			printf("Write 0x%04lX: %d @ %f c (TIMA = %d)\n", address, value, g_totalCyclesExecuted, TIMA);
			int x = 3;
		}

		switch (address)
		{
		case MemoryMappedRegisters::DIV: value = 0; break;
			
			// Serial output
		case MemoryMappedRegisters::SC:
			{
				//@TODO: timing emulation
				if (value & Bit7)
				{
					//printf("Serial output byte: %c (0x%02lX)\n", m_SB, m_SB);
					printf("%c", SB);
					SC &= ~Bit7;
					// Read zeroes
					SB = 0;
				}
			}
		}

		switch (address)
		{
			// Handle writes to all the memory-mapped registers
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) case MemoryMappedRegisters::name: MMR_NAME(name) = value; break;
#define DEFINE_MEMORY_MAPPED_REGISTER_R(addx, name) case MemoryMappedRegisters::name: throw Exception("Write to read-only register"); break;
#define DEFINE_MEMORY_MAPPED_REGISTER_W(addx, name) case MemoryMappedRegisters::name: MMR_NAME(name) = value; break;
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW
#undef DEFINE_MEMORY_MAPPED_REGISTER_R
#undef DEFINE_MEMORY_MAPPED_REGISTER_W
		default:
			throw NotImplementedException();
		}
	}
	
	std::shared_ptr<Rom> m_pRom;
	OperationMode m_mode;
	char m_vram[kVramSize];
	char m_workMemory[kWorkMemorySize];
	char m_hram[kHramMemorySize];
};
