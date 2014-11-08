#pragma once

#include "Rom.h"

#include <memory>

#include "SDL.h"

//class Mapper

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
	static const int kHramMemorySize = 0xFF; // last byte is IE register

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

	Memory(const std::shared_ptr<Rom>& rom)
		: m_pRom(rom)
	{
		m_mode = static_cast<OperationMode>(rom->GetRom()[0x147]);
		SDL_assert((m_mode == OperationMode::RomOnly) || (m_mode == OperationMode::MBC1));
		Reset();
	}

	void Reset()
	{
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) m_##name = 0x00;
#define DEFINE_MEMORY_MAPPED_REGISTER_R(addx, name) m_##name = 0x00;
#define DEFINE_MEMORY_MAPPED_REGISTER_W(addx, name) m_##name = 0x00;
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW
#undef DEFINE_MEMORY_MAPPED_REGISTER_R
#undef DEFINE_MEMORY_MAPPED_REGISTER_W
	}

	Uint8 Read8(Uint16 address, bool throwIfUnknown = true, bool* pSuccess = nullptr)
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
			switch (address)
			{
				// Handle reads to all the memory-mapped registers
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) case MemoryMappedRegisters::name: return m_##name;
#define DEFINE_MEMORY_MAPPED_REGISTER_R(addx, name) case MemoryMappedRegisters::name: return m_##name;
#define DEFINE_MEMORY_MAPPED_REGISTER_W(addx, name) case MemoryMappedRegisters::name: throw Exception("Read from write-only register"); break;
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW
#undef DEFINE_MEMORY_MAPPED_REGISTER_R
#undef DEFINE_MEMORY_MAPPED_REGISTER_W
			default:
				if (throwIfUnknown)
				{
					throw NotImplementedException();
				}
				*pSuccess = false;
				return 0xFF;
			}
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

private:

	bool IsAddressInRange(Uint16 address, Uint16 base, Uint16 rangeSize)
	{
		return (address >= base) && (address < base + rangeSize);
	}

	void WriteMemoryMappedRegister(Uint16 address, Uint8 value)
	{
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
					printf("%c", m_SB);
					m_SC &= ~Bit7;
					// Read zeroes
					m_SB = 0;
				}
			}
		}

		switch (address)
		{
			// Handle writes to all the memory-mapped registers
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) case MemoryMappedRegisters::name: m_##name = value; break;
#define DEFINE_MEMORY_MAPPED_REGISTER_R(addx, name) case MemoryMappedRegisters::name: throw Exception("Write to read-only register"); break;
#define DEFINE_MEMORY_MAPPED_REGISTER_W(addx, name) case MemoryMappedRegisters::name: m_##name = value; break;
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW
#undef DEFINE_MEMORY_MAPPED_REGISTER_R
#undef DEFINE_MEMORY_MAPPED_REGISTER_W
		default:
			throw NotImplementedException();
		}
	}
	
	// Declare storage for all the memory-mapped registers
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) Uint8 m_##name;
#define DEFINE_MEMORY_MAPPED_REGISTER_R(addx, name) Uint8 m_##name;
#define DEFINE_MEMORY_MAPPED_REGISTER_W(addx, name) Uint8 m_##name;
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW
#undef DEFINE_MEMORY_MAPPED_REGISTER_R
#undef DEFINE_MEMORY_MAPPED_REGISTER_W

	std::shared_ptr<Rom> m_pRom;
	OperationMode m_mode;
	char m_vram[kVramSize];
	char m_workMemory[kWorkMemorySize];
	char m_hram[kHramMemorySize];
};
