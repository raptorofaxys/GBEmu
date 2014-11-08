#pragma once

#include "Rom.h"

#include <memory>

#include "SDL.h"

class Memory
{
public:
	static const int kRomBase = 0x0000;
	static const int kRomBaseBank00Size = 0x4000;
	
	static const int kWorkMemoryBase = 0xC000;
	static const int kWorkMemorySize = 0x2000; // 8k

	static const int kHramMemoryBase = 0xFF80;
	static const int kHramMemorySize = 0xFF; // last byte is IE register

	// Define memory addresses for all the memory-mapped registers
	enum class MemoryMappedRegisters
	{
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) name = addx,
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW
	};

	Memory(const std::shared_ptr<Rom>& rom)
		: m_pRom(rom)
	{
	}

	Uint8 Read8(Uint16 address)
	{
		if (IsAddressInRange(address, kRomBase, kRomBaseBank00Size))
		{
			return m_pRom->GetRom()[address];
		}
		else if (IsAddressInRange(address, kWorkMemoryBase, kWorkMemorySize))
		{
			return m_workMemory[address - kWorkMemoryBase];
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
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW
			default:
				throw NotImplementedException();
			}
		}
	}
	
	bool SafeRead8(Uint16 address, Uint8& value)
	{
		try
		{
			value = Read8(address);
			return true;
		}
		catch (const Exception&)
		{
			return false;
		}
	}

	Uint16 Read16(Uint16 address)
	{
		// Must handle as two reads, because the address can cross range boundaries
		return Make16(Read8(address + 1), Read8(address));
	}

	void Write8(Uint16 address, Uint8 value)
	{
		if (IsAddressInRange(address, kRomBase, kRomBaseBank00Size))
		{
			throw Exception("Attempted to write to ROM area.");
		}
		else if (IsAddressInRange(address, kWorkMemoryBase, kWorkMemorySize))
		{
			m_workMemory[address - kWorkMemoryBase] = value;
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
		}

		switch (address)
		{
			// Handle writes to all the memory-mapped registers
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) case MemoryMappedRegisters::name: m_##name = value; break;
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW
		default:
			throw NotImplementedException();
		}
	}
	
	// Declare storage for all the memory-mapped registers
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) Uint8 m_##name;
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW

	std::shared_ptr<Rom> m_pRom;
	char m_workMemory[kWorkMemorySize];
	char m_hram[kHramMemorySize];
};
