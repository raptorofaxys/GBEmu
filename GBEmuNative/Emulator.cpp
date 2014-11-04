#include "Utils.h"

#include "SDL.h"

#include <stdlib.h>
#include <stdio.h>
#include <memory>
#include <vector>

#include <Windows.h>

void LoadFileAsByteArray(std::vector<Uint8>& output, const char* pFileName)
{
    SDL_RWops* pHandle = SDL_RWFromFile(pFileName, "rb");
    
	if (!pHandle)
	{
        throw Exception("Failed to load file %s.", pFileName);
    }

	SDL_RWseek(pHandle, 0,  RW_SEEK_END);
	int fileSize = static_cast<int>(SDL_RWtell(pHandle));
	SDL_RWseek(pHandle, 0,  RW_SEEK_SET);

	output.resize(fileSize);
	SDL_RWread(pHandle, output.data(), fileSize, 1);
    SDL_RWclose(pHandle);
}

std::shared_ptr<std::vector<Uint8>> LoadFileAsByteArray(const char* pFileName)
{
	std::shared_ptr<std::vector<Uint8>> pData(new std::vector<Uint8>);
	LoadFileAsByteArray(*pData, pFileName);

	return pData;
}

class Rom
{
public:
	Rom(const char* pFileName)
	{
		LoadFromFile(pFileName);
	}

	std::string GetRomName() const
	{
		std::string result;

		const Uint8* p = &m_pRom[kNameOffset];
		for (int i = 0; i < kNameLength; ++i)
		{
			if (*p)
			{
				result += *p;
			}
			++p;
		}

		return result;
	}

	const std::vector<Uint8>& GetRom()
	{
		return m_pRom;
	}

private:
	static const int kNameOffset = 0x134;
	static const int kNameLength = 0x11;

	void LoadFromFile(const char* pFileName)
	{
		LoadFileAsByteArray(m_pRom, pFileName);
	}

	std::vector<Uint8> m_pRom;
};

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

class Cpu
{
public:
	static Uint32 const kCyclesPerSecond = 4194304;

	enum class FlagBitIndex
	{
		Zero = 7,
		Subtract = 6,
		HalfCarry = 5,
		Carry = 4
	};

	Cpu(const std::shared_ptr<Memory>& memory)
		: m_pMemory(memory)
	{
		Reset();
	}

	void Reset()
	{
		m_cyclesRemaining = 0.0f;
		m_totalOpcodesExecuted = 0;

		IME = true;

		static_assert(offsetof(Cpu, F) == offsetof(Cpu, AF), "Target machine is not little-endian; register unions must be revised");
		PC = 0x0100;
		AF = 0x01B0;
		BC = 0x0013;
		DE = 0x00D8;
		HL = 0x014D;

		//@TODO: initial state
		//Write8(Memory::MemoryMappedRegisters::TIMA, 0);
	}

	//template <int N>
	//struct OR
	//{
	//	void Do(Cpu& cpu)
	//	{
	//		case 0xB1:
	//		case 0xB2:
	//		case 0xB3:
	//		case 0xB4:
	//		case 0xB5:
	//		case 0xB6:
	//		case 0xB7:
	//			{
	//				C = 4;
	//				Uint8 value = 0;
	//				switch (opcode & 0x7)
	//				{
	//				case 0: value = B; break;
	//				case 1: value = C; break;
	//				case 2: value = D; break;
	//				case 3: value = E; break;
	//				case 4: value = H; break;
	//				case 5: value = L; break;
	//				case 6: value = Read8(HL); C += 4; break;
	//				case 7: value = A; break;
	//				}

	//				A |= value;
	//				SetZeroFromValue(A);
	//				ClearSubtract();
	//				ClearHalfCarry();
	//				ClearCarry();
	//			}
	//			break;
	//	}
	//};

	template <int N> struct b0_2 { enum { Value = N & 0x7 }; };
	template <int N> struct b4_5 { enum { Value = (N >> 4) & 0x3 }; };

	// This reads: parse bits 0 to 2, select from B, C, D, E, H, L, indirect HL, A
	template <int N> Uint8 b0_2_B_C_D_E_H_L_iHL_A_Read8_Impl();
	template <> Uint8 b0_2_B_C_D_E_H_L_iHL_A_Read8_Impl<0>() { return B; }
	template <> Uint8 b0_2_B_C_D_E_H_L_iHL_A_Read8_Impl<1>() { return C; }
	template <> Uint8 b0_2_B_C_D_E_H_L_iHL_A_Read8_Impl<2>() { return D; }
	template <> Uint8 b0_2_B_C_D_E_H_L_iHL_A_Read8_Impl<3>() { return E; }
	template <> Uint8 b0_2_B_C_D_E_H_L_iHL_A_Read8_Impl<4>() { return H; }
	template <> Uint8 b0_2_B_C_D_E_H_L_iHL_A_Read8_Impl<5>() { return L; }
	template <> Uint8 b0_2_B_C_D_E_H_L_iHL_A_Read8_Impl<6>() { return Read8(HL); }
	template <> Uint8 b0_2_B_C_D_E_H_L_iHL_A_Read8_Impl<7>() { return A; }
	template <int N> Uint8 b0_2_B_C_D_E_H_L_iHL_A_Read8() { return b0_2_B_C_D_E_H_L_iHL_A_Read8_Impl<b0_2<N>::Value>(); }

	template <int N> Uint16& b4_5_BC_DE_HL_SP_GetReg16();
	template <> Uint16& b4_5_BC_DE_HL_SP_GetReg16<0>() { return BC; }
	template <> Uint16& b4_5_BC_DE_HL_SP_GetReg16<1>() { return DE; }
	template <> Uint16& b4_5_BC_DE_HL_SP_GetReg16<2>() { return HL; }
	template <> Uint16& b4_5_BC_DE_HL_SP_GetReg16<3>() { return SP; }
	//template <int N> Uint16 b4_5_BC_DE_HL_SP_Read16_Impl() { return b4_5_BC_DE_HL_SP_GetReg16<N>(); }
	//template <int N> Uint16 b4_5_BC_DE_HL_SP_Read16() { return b4_5_BC_DE_HL_SP_Read16_Impl<b4_5<N>::Value>(); }
	template <int N> void b4_5_BC_DE_HL_SP_Write16_Impl(Uint16 value) { b4_5_BC_DE_HL_SP_GetReg16<N>() = value; }
	template <int N> void b4_5_BC_DE_HL_SP_Write16(Uint16 value) { b4_5_BC_DE_HL_SP_Write16_Impl<b4_5<N>::Value>(value); }

			//case 0x01: // LD ?,nn
			//case 0x11:
			//case 0x21:
			//case 0x31:
			//	{
			//		instructionCycles = 12;
			//		auto value = Fetch16();
			//		switch ((opcode >> 4) & 0x3)
			//		{
			//		case 0: BC = value; break;
			//		case 1: DE = value; break;
			//		case 2: HL = value; break;
			//		case 3: SP = value; break;
			//		}
			//	}
			//	break;

	template <int N> void LD_0_3__1()
	{
		//b4_5_BC_DE_HL_SP_Read16<N>();
		b4_5_BC_DE_HL_SP_Write16<N>(Fetch16());
	}

	// @TODO: test an iHL exception in the GetReg approach

	void OR(Uint8 value)
	{
		A |= value;
		SetZeroFromValue(A);
		ClearSubtract();
		ClearHalfCarry();
		ClearCarry();
	}

	// This reads: OR opcode, starting with 0xBn, with lower nibble values 0-7
	template <int N> void OR_B__0_7()
	{
		OR(b0_2_B_C_D_E_H_L_iHL_A_Read8<N>());
	}
	
	// OR opcode F6
	template <int N> void OR_F6()
	{
		OR(Fetch8());
	}
	
	void Update(float seconds)
	{
		m_cyclesRemaining += seconds * kCyclesPerSecond;

		// We have a few options for implementing opcode lookup and execution.  My goals are:
		// -first, to have fun with C++
		// -next, to avoid repetition, thus factoring as much as possible
		// -lastly, to generate relatively good code if that doesn't mean intefering with the previous two goals
		//
		// I do care about readability and maintainability, but this is a weekend exercise with no other developers, and I don't mind winding up with something a bit abstruse if it means the above goals are met.
		// 
		// With the above in mind, the brute force implementation approach holds relatively little interest.  What I would like to do is to capture the underlying PLA-like structure of the hardware, where multiplexing
		// circuitry is reused between different opcodes.  Many things are computed in parallel in the hardware, and the microcode of each opcode acts as a sequencer of sorts, picking and choosing which
		// outputs get routed to which inputs, and so on.
		// 
		// Each sub-circuit could be represented by a small function, and read/write access could be routed through virtual functions.  This would be slow and relatively mundane.  Instead of the double indirection
		// of virtual functions, we could simply use std::function (or raw function pointers), but that still means runtime branching.
		// What's interesting is that the opcode case expression is constant, which means we could use template logic to get the compiler to generate the appropriate code for each individual opcode based on the case expression.
		// This requires
		
		while (m_cyclesRemaining > 0)
		{
			Uint8 opcode = Fetch8();

			Sint32 instructionCycles = -1; // number of clock cycles used by the opcode

			//SInt32 SourceCycles = 0;

#define OPCODE(code, cycles, name) case code: instructionCycles = (cycles); name<code>(); break;
			switch (opcode)
			{
				//@TODO: this will undoubtedly grow and be refactored.  Just want to have a few use cases to factor right.  Lambdas for PLA-like microcode?
			case 0x00: instructionCycles = 4; break; // NOP

			case 0x18: // JR n
				{
					instructionCycles = 8;
					PC += Fetch8();
				}
				break;
			
			OPCODE(0x01, 12, LD_0_3__1)
			OPCODE(0x11, 12, LD_0_3__1)
			OPCODE(0x21, 12, LD_0_3__1)
			OPCODE(0x31, 12, LD_0_3__1)

			case 0x3E: instructionCycles = 8; A = Fetch8(); break; // LD A,n
			
			case 0x2A: // LDI A,(HL)
				{
					instructionCycles = 8;
					A = Read8(HL);
					++HL;
				}
				break;

			case 0x03: // INC ?
			case 0x13:
			case 0x23:
			case 0x33:
				{
					instructionCycles = 8;
					//@TODO: refactor with DEC 0x0B, LD 0x01, etc.?
					switch ((opcode >> 4) & 0x3)
					{
					case 0: ++BC; break;
					case 1: ++DE; break;
					case 2: ++HL; break;
					case 3: ++SP; break;
					}
				}
				break;

			case 0x0B: // DEC ?
			case 0x1B:
			case 0x2B:
			case 0x3B:
				{
					instructionCycles = 8;
					switch ((opcode >> 4) & 0x3)
					{
					case 0: --BC; break;
					case 1: --DE; break;
					case 2: --HL; break;
					case 3: --SP; break;
					}
				}
				break;

			//case 0x78: instructionCycles = 4; A = B; break; // LD A,B
			//case 0x79: instructionCycles = 4; A = C; break; // LD A,C
			//case 0x7A: instructionCycles = 4; A = D; break; // LD A,D
			//case 0x7B: instructionCycles = 4; A = E; break; // LD A,E
			//case 0x7C: instructionCycles = 4; A = H; break; // LD A,H
			//case 0x7D: instructionCycles = 4; A = L; break; // LD A,L
			//case 0x7E: instructionCycles = 8; A = Read8(HL); break; // LD A,(HL)
			//case 0x7F: instructionCycles = 4; A = A; break; // LD A,A
				// LD A,?
			case 0x40:
			case 0x41:
			case 0x42:
			case 0x43:
			case 0x44:
			case 0x45:
			case 0x46:
			case 0x47:
			case 0x48:
			case 0x49:
			case 0x4A:
			case 0x4B:
			case 0x4C:
			case 0x4D:
			case 0x4E:
			case 0x4F:
			case 0x50:
			case 0x51:
			case 0x52:
			case 0x53:
			case 0x54:
			case 0x55:
			case 0x56:
			case 0x57:
			case 0x58:
			case 0x59:
			case 0x5A:
			case 0x5B:
			case 0x5C:
			case 0x5D:
			case 0x5E:
			case 0x5F:
			case 0x60:
			case 0x61:
			case 0x62:
			case 0x63:
			case 0x64:
			case 0x65:
			case 0x66:
			case 0x67:
			case 0x68:
			case 0x69:
			case 0x6A:
			case 0x6B:
			case 0x6C:
			case 0x6D:
			case 0x6E:
			case 0x6F:
			case 0x70:
			case 0x71:
			case 0x72:
			case 0x73:
			case 0x74:
			case 0x75:
			//case 0x76: // 0x76 is HALT
			case 0x77:
			case 0x78:
			case 0x79:
			case 0x7A:
			case 0x7B:
			case 0x7C:
			case 0x7D:
			case 0x7E:
			case 0x7F:
				{
					instructionCycles = 4;
					Uint8 value = 0;
					switch (opcode & 0x7)
					{
					case 0: value = B; break;
					case 1: value = C; break;
					case 2: value = D; break;
					case 3: value = E; break;
					case 4: value = H; break;
					case 5: value = L; break;
					case 6: value = Read8(HL); instructionCycles += 4; break;
					case 7: value = A; break;
					}

					switch ((opcode >> 3) & 0x7)
					{
					case 0: B = value; break;
					case 1: instructionCycles = value; break;
					case 2: D = value; break;
					case 3: E = value; break;
					case 4: H = value; break;
					case 5: L = value; break;
					case 6: Write8(HL, value); instructionCycles += 4; break;
					case 7: A = value; break;
					}
				}
				break;
			//case 0x76: // 0x76 is HALT

			// OR ?
			OPCODE(0xB0, 4, OR_B__0_7)
			OPCODE(0xB1, 4, OR_B__0_7)
			OPCODE(0xB2, 4, OR_B__0_7)
			OPCODE(0xB3, 4, OR_B__0_7)
			OPCODE(0xB4, 4, OR_B__0_7)
			OPCODE(0xB5, 4, OR_B__0_7)
			OPCODE(0xB6, 8, OR_B__0_7)
			OPCODE(0xB7, 4, OR_B__0_7)
			OPCODE(0xF6, 8, OR_F6)
			//case 0xB0: instructionCycles = 4; OR<0xB0>(); break;
			//case 0xB1: instructionCycles = 4; OR<0xB1>(); break;
			//case 0xB2: instructionCycles = 4; OR<0xB2>(); break;
			//case 0xB3: instructionCycles = 4; OR<0xB3>(); break;
			//case 0xB4: instructionCycles = 4; OR<0xB4>(); break;
			//case 0xB5: instructionCycles = 4; OR<0xB5>(); break;
			//case 0xB6: instructionCycles = 8; OR<0xB6>(); break;
			//case 0xB7: instructionCycles = 4; OR<0xB7>(); break;
			//case 0xB0: OR<B0>::Do(*this); break;
			//case 0xB1: OR<B1>::Do(*this); break;
			//case 0xB2: OR<B2>::Do(*this); break;
			//case 0xB3: OR<B3>::Do(*this); break;
			//case 0xB4: OR<B4>::Do(*this); break;
			//case 0xB5: OR<B5>::Do(*this); break;
			//case 0xB6: OR<B6>::Do(*this); break;
			//case 0xB7: OR<B7>::Do(*this); break;
			
			case 0xC1: // POP ?
			case 0xD1:
			case 0xE1:
			case 0xF1:
				{
					instructionCycles = 12;
					Uint16 value = 0;
					//@TODO: refactor with Push? must dissociate the notion of reading and writing from the address itself... like for (HL) in LDs
					switch ((opcode >> 4) & 0x3)
					{
					case 0: BC = Pop16(); break;
					case 1: DE = Pop16(); break;
					case 2: HL = Pop16(); break;
					case 3: AF = Pop16(); break;
					}
				}
				break;
			
			case 0xC5: // PUSH ?
			case 0xD5:
			case 0xE5:
			case 0xF5:
				{
					instructionCycles = 16;
					Uint16 value = 0;
					switch ((opcode >> 4) & 0x3)
					{
					case 0: Push16(BC); break;
					case 1: Push16(DE); break;
					case 2: Push16(HL); break;
					case 3: Push16(AF); break;
					}
				}
				break;
			
			case 0xC3: // JP nn
				{
					instructionCycles = 12;
					auto target = Fetch16();
					PC = target;
				}
				break;
			case 0xC9: // RET
				{
					instructionCycles = 8;
					PC = Pop16();
				}
				break;
			case 0xCD: // CALL nn
				{
					instructionCycles = 12;
					Push16(PC + 2);
					PC = Fetch16();
				}
				break;
			case 0xE0: // LD (0xFF00+n),A
				{
					instructionCycles = 12;
					auto disp = Fetch8();
					auto address = disp + 0xFF00;
					Write8(address, A);
				}
				break;
			case 0xEA: // LD (nn),A
				{
					instructionCycles = 16;
					auto address = Fetch16();
					Write8(address, A);
				}
				break;
			case 0xF3: // DI
				{
					instructionCycles = 4;
					IME = false;
				}
				break;
			case 0xD3:
			case 0xDB:
			case 0xDD:
			case 0xE3:
			case 0xE4:
			case 0xEB:
			case 0xEC:
			case 0xED:
			case 0xF2:
			case 0xF4:
			case 0xFC:
			case 0xFD:
				throw Exception("Illegal opcode executed: 0x%02lX", opcode);
				break;

#undef OPCODE
			default:
				SDL_assert(false && "Unknown opcode encountered");
			}

			SDL_assert(instructionCycles != -1);
			m_cyclesRemaining -= instructionCycles;
			++m_totalOpcodesExecuted;
		}
	}

private:
	Uint8 Fetch8()
	{
		auto result = m_pMemory->Read8(PC);
		++PC;
		return result;
	}

	Uint16 Fetch16()
	{
		auto result = m_pMemory->Read16(PC);
		PC += 2;
		return result;
	}

	Uint8 Read8(Uint16 address)
	{
		return m_pMemory->Read8(address);
	}

	void Write8(Uint16 address, Uint8 value)
	{
		m_pMemory->Write8(address, value);
	}

	void Push16(Uint16 value)
	{
		SP -= 2;
		m_pMemory->Write16(SP, value);
	}

	Uint16 Pop16()
	{
		auto result = m_pMemory->Read16(SP);
		SP += 2;
		return result;
	}

	void SetZeroFromValue(Uint8 value)
	{
		SetFlagValue(FlagBitIndex::Zero, value == 0);
	}

	void ClearSubtract()
	{
		SetFlagValue(FlagBitIndex::Subtract, false);
	}

	void ClearHalfCarry()
	{
		SetFlagValue(FlagBitIndex::HalfCarry, false);
	}

	void ClearCarry()
	{
		SetFlagValue(FlagBitIndex::Carry, false);
	}

	void SetFlagValue(FlagBitIndex position, bool value)
	{
		auto bitMask = (1 << static_cast<Uint8>(position));
		F = value ? (F | bitMask) : (F & ~bitMask);
	}

	// This macro helps define register pairs that have alternate views.  For example, B and C can be indexed individually as 8-bit registers, but they can also be indexed together as a 16-bit register called BC. 
	// A static_assert helps make sure the machine endianness behaves as expected.
#define DualViewRegisterPair(High, Low) \
	union \
	{ \
		struct \
		{ \
			Uint8 Low; \
			Uint8 High; \
		}; \
		Uint16 High##Low; \
	};

    DualViewRegisterPair(A, F)
    DualViewRegisterPair(B, C)
    DualViewRegisterPair(D, E)
    DualViewRegisterPair(H, L)

#undef DualViewRegisterPair

	Uint16 SP;
	Uint16 PC;
	bool IME; // whether interrupts are enabled - very special register, not memory-mapped

	float m_cyclesRemaining;
	Uint32 m_totalOpcodesExecuted;

	std::shared_ptr<Memory> m_pMemory;
};

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

int main(int argc, char **argv)
{
	try
	{
		ProcessConsole console;

		SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			throw Exception("Couldn't initialize SDL: %s", SDL_GetError());
		}

		Janitor j([] { SDL_Quit(); });

		//GameBoy gb("Tetris (JUE) (V1.1) [!].gb");
		GameBoy gb("cpu_instrs\\cpu_instrs.gb");
		//Rom rom("Tetris (JUE) (V1.1) [!].gb");
		const auto& gameName = gb.GetRom().GetRomName();

		std::shared_ptr<SDL_Window> pWindow(SDL_CreateWindow(gameName.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, GameBoy::kScreenWidth * 4, GameBoy::kScreenHeight * 4, 0), SDL_DestroyWindow);

		if (!pWindow)
		{
			throw Exception("Couldn't create window");
		}
		
		std::shared_ptr<SDL_Renderer> pRenderer(SDL_CreateRenderer(pWindow.get(), -1, 0), SDL_DestroyRenderer);
		if (!pRenderer)
		{
			throw Exception("Couldn't create renderer");
		}

		std::shared_ptr<SDL_Texture> pFrameBuffer(SDL_CreateTexture(pRenderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, GameBoy::kScreenWidth, GameBoy::kScreenHeight), SDL_DestroyTexture);
		if (!pRenderer)
		{
			throw Exception("Couldn't create framebuffer texture");
		}

		SDL_Event event;
		bool done = false;

		Uint32 lastTicks = SDL_GetTicks();
		while (!done)
		{
		    while (SDL_PollEvent(&event))
			{
		        switch (event.type)
				{
		        case SDL_KEYDOWN:
		            if (event.key.keysym.sym == SDLK_ESCAPE)
					{
		                done = true;
		            }
		            break;
		        case SDL_QUIT:
		            done = true;
		            break;
		        }
		    }

			Uint32 ticks = SDL_GetTicks();
			gb.Update((ticks - lastTicks) / 1000.0f);
			lastTicks = ticks;

			void* pPixels;
			int pitch;
			SDL_LockTexture(pFrameBuffer.get(), NULL, &pPixels, &pitch);
			Uint32* pARGB = static_cast<Uint32*>(pPixels);
			*pARGB = rand();
			SDL_UnlockTexture(pFrameBuffer.get());

		    SDL_RenderClear(pRenderer.get());
		    SDL_RenderCopy(pRenderer.get(), pFrameBuffer.get(), NULL, NULL);
		    SDL_RenderPresent(pRenderer.get());
		}
	}
	catch (const Exception& e)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Exception: %s", e.GetMessage());
	}

    return 0;
}