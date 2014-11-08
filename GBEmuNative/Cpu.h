#pragma once

#include "Memory.h"

#include <memory>

#include "SDL.h"

enum class FlagBitIndex
{
	Zero = 7,
	Subtract = 6,
	HalfCarry = 5,
	Carry = 4,
};

namespace FlagBitMask
{
	enum Type
	{
		Zero = (1 << static_cast<int>(FlagBitIndex::Zero)),
		Subtract = (1 << static_cast<int>(FlagBitIndex::Subtract)),
		HalfCarry = (1 << static_cast<int>(FlagBitIndex::HalfCarry)),
		Carry = (1 << static_cast<int>(FlagBitIndex::Carry)),
		All = Zero | Subtract | HalfCarry | Carry,
	};
}

class Cpu
{
public:
	static Uint32 const kCyclesPerSecond = 4194304;

	enum class DebuggerState
	{
		Running,
		SingleStepping
	};

	Cpu(const std::shared_ptr<Memory>& memory)
		: m_pMemory(memory)
	{
		m_debuggerState = DebuggerState::SingleStepping;
		Reset();
	}

	void Reset()
	{
		m_cyclesRemaining = 0.0f;
		m_totalOpcodesExecuted = 0;

		m_cpuHalted = false;

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

#define VERIFY_OPCODE() SDL_TriggerBreakpoint()
//#define VERIFY_OPCODE()
	
	///////////////////////////////////////////////////////////////////////////
	// Bit parsing template metafunctions
	///////////////////////////////////////////////////////////////////////////

	template <int N> struct b0_2 { enum { Value = N & 0x7 }; };
	template <int N> struct b3_4 { enum { Value = (N >> 3) & 0x3 }; };
	template <int N> struct b3_5 { enum { Value = (N >> 3) & 0x7 }; };
	template <int N> struct b4 { enum { Value = (N >> 4) & 0x1 }; };
	template <int N> struct b4_5 { enum { Value = (N >> 4) & 0x3 }; };

	///////////////////////////////////////////////////////////////////////////
	// Micro-opcode implementations
	///////////////////////////////////////////////////////////////////////////

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

	template <int N> bool b3_4_NZ_Z_NC_C_Eval_Impl();
	template <> bool b3_4_NZ_Z_NC_C_Eval_Impl<0>() { return !GetFlagValue(FlagBitIndex::Zero); } 
	template <> bool b3_4_NZ_Z_NC_C_Eval_Impl<1>() { return GetFlagValue(FlagBitIndex::Zero); } 
	template <> bool b3_4_NZ_Z_NC_C_Eval_Impl<2>() { return !GetFlagValue(FlagBitIndex::Carry); } 
	template <> bool b3_4_NZ_Z_NC_C_Eval_Impl<3>() { return GetFlagValue(FlagBitIndex::Carry); } 
	template <int N> bool b3_4_NZ_Z_NC_C_Eval() { return b3_4_NZ_Z_NC_C_Eval_Impl<b3_4<N>::Value>(); }

	template <int N> Uint8& b3_5_B_C_D_E_H_L_iHL_A_GetReg8();
	template <> Uint8& b3_5_B_C_D_E_H_L_iHL_A_GetReg8<0>() { return B; }
	template <> Uint8& b3_5_B_C_D_E_H_L_iHL_A_GetReg8<1>() { return C; }
	template <> Uint8& b3_5_B_C_D_E_H_L_iHL_A_GetReg8<2>() { return D; }
	template <> Uint8& b3_5_B_C_D_E_H_L_iHL_A_GetReg8<3>() { return E; }
	template <> Uint8& b3_5_B_C_D_E_H_L_iHL_A_GetReg8<4>() { return H; }
	template <> Uint8& b3_5_B_C_D_E_H_L_iHL_A_GetReg8<5>() { return L; }
	template <> Uint8& b3_5_B_C_D_E_H_L_iHL_A_GetReg8<7>() { return A; }
	template <int N> Uint16 b3_5_B_C_D_E_H_L_iHL_A_GetAddress();
	template <> Uint16 b3_5_B_C_D_E_H_L_iHL_A_GetAddress<6>() { return HL; }
	template <int N> Uint8 b3_5_B_C_D_E_H_L_iHL_A_Read8_Impl() { return b3_5_B_C_D_E_H_L_iHL_A_GetReg8<N>(); }
	template <> Uint8 b3_5_B_C_D_E_H_L_iHL_A_Read8_Impl<6>() { VERIFY_OPCODE(); return Read8(b3_5_B_C_D_E_H_L_iHL_A_GetAddress<6>()); }
	template <int N> Uint8 b3_5_B_C_D_E_H_L_iHL_A_Read8() { return b3_5_B_C_D_E_H_L_iHL_A_Read8_Impl<b3_5<N>::Value>(); }
	template <int N> void b3_5_B_C_D_E_H_L_iHL_A_Write8_Impl(Uint8 value) { b3_5_B_C_D_E_H_L_iHL_A_GetReg8<N>() = value; }
	template <> void b3_5_B_C_D_E_H_L_iHL_A_Write8_Impl<6>(Uint8 value) { VERIFY_OPCODE(); Write8(b3_5_B_C_D_E_H_L_iHL_A_GetAddress<6>(), value); }
	template <int N> void b3_5_B_C_D_E_H_L_iHL_A_Write8(Uint8 value) { b3_5_B_C_D_E_H_L_iHL_A_Write8_Impl<b3_5<N>::Value>(value); }

	template <int N> Uint16 b4_iBC_iDE_GetAddress();
	template <> Uint16 b4_iBC_iDE_GetAddress<0>() { VERIFY_OPCODE(); return BC; }
	template <> Uint16 b4_iBC_iDE_GetAddress<1>() { VERIFY_OPCODE(); return DE; }
	template <int N> Uint8 b4_iBC_iDE_Read8_Impl() { VERIFY_OPCODE(); return Read8(b4_iBC_iDE_GetAddress<N>()); }
	template <int N> Uint8 b4_iBC_iDE_Read8() { return b4_iBC_iDE_Read8_Impl<b4<N>::Value>(); }
	template <int N> void b4_iBC_iDE_Write8_Impl(Uint8 value) { VERIFY_OPCODE(); Write8(b4_iBC_iDE_GetAddress<N>(), value); }
	template <int N> void b4_iBC_iDE_Write8(Uint8 value) { b4_iBC_iDE_Write8_Impl<b4<N>::Value>(value); }

	template <int N> Uint16& b4_5_BC_DE_HL_SP_GetReg16();
	template <> Uint16& b4_5_BC_DE_HL_SP_GetReg16<0>() { return BC; }
	template <> Uint16& b4_5_BC_DE_HL_SP_GetReg16<1>() { return DE; }
	template <> Uint16& b4_5_BC_DE_HL_SP_GetReg16<2>() { return HL; }
	template <> Uint16& b4_5_BC_DE_HL_SP_GetReg16<3>() { return SP; }
	template <int N> Uint16 b4_5_BC_DE_HL_SP_Read16_Impl() { return b4_5_BC_DE_HL_SP_GetReg16<N>(); }
	template <int N> Uint16 b4_5_BC_DE_HL_SP_Read16() { return b4_5_BC_DE_HL_SP_Read16_Impl<b4_5<N>::Value>(); }
	template <int N> void b4_5_BC_DE_HL_SP_Write16_Impl(Uint16 value) { b4_5_BC_DE_HL_SP_GetReg16<N>() = value; }
	template <int N> void b4_5_BC_DE_HL_SP_Write16(Uint16 value) { b4_5_BC_DE_HL_SP_Write16_Impl<b4_5<N>::Value>(value); }

	///////////////////////////////////////////////////////////////////////////
	// Opcode implementations
	//
	///////////////////////////////////////////////////////////////////////////

	// Function names read like so:
	// template <int N> void INC_0_3__4__0_3__C()
	// That means:
	// Instruction INC, from high nibble 0 to high nibble 3, low nibble 4 (i.e. 0x04, 0x14, 0x24, 0x34); also from high nibble 0 to high nibble 3, low nibble C (i.e. 0x0C, 0x1C, 0x2C, 0x3C)
	//
	// The block in the update loop that invokes the above looks like this, forwarding all the opcodes to the same handler:
	//	OPCODE(0x04, 4, INC_0_3__4__0_3__C)
	//	OPCODE(0x0C, 4, INC_0_3__4__0_3__C)
	//	OPCODE(0x14, 4, INC_0_3__4__0_3__C)
	//	OPCODE(0x1C, 4, INC_0_3__4__0_3__C)
	//	OPCODE(0x24, 4, INC_0_3__4__0_3__C)
	//	OPCODE(0x2C, 4, INC_0_3__4__0_3__C)
	//	OPCODE(0x34, 8, INC_0_3__4__0_3__C)
	//	OPCODE(0x3C, 4, INC_0_3__4__0_3__C)

	void OR(Uint8 value)
	{
		A |= value;
		SetZeroFromValue(A);
		SetFlagValue(FlagBitIndex::Subtract, false);
		SetFlagValue(FlagBitIndex::HalfCarry, false);
		SetFlagValue(FlagBitIndex::Carry, false);
	}

	template <int N> void NOP()
	{
	}

	template <int N> void LD_0_1__A()
	{
		SDL_TriggerBreakpoint();
		A = b4_iBC_iDE_Read8<N>();
	}

	template <int N> void LD_0_3__1()
	{
		b4_5_BC_DE_HL_SP_Write16<N>(Fetch16());
	}

	template <int N> void INC_0_3__3()
	{
		b4_5_BC_DE_HL_SP_Write16<N>(b4_5_BC_DE_HL_SP_Read16<N>() + 1);
	}

	template <int N> void INC_0_3__4__0_3__C()
	{
		auto oldValue = b3_5_B_C_D_E_H_L_iHL_A_Read8<N>();
		auto newValue = oldValue + 1;
		b3_5_B_C_D_E_H_L_iHL_A_Write8<N>(newValue);
		SetFlagsForAdd(oldValue, 1, FlagBitMask::Zero | FlagBitMask::Subtract | FlagBitMask::HalfCarry);
	}

	template <int N> void DEC_0_3__5__0_3__D()
	{
		auto oldValue = b3_5_B_C_D_E_H_L_iHL_A_Read8<N>();
		auto newValue = oldValue - 1;
		b3_5_B_C_D_E_H_L_iHL_A_Write8<N>(newValue);
		SetFlagsForSub(oldValue, 1, FlagBitMask::Zero | FlagBitMask::Subtract | FlagBitMask::HalfCarry);
	}

	template <int N> void LD_0_3__6__0_3__E()
	{
		b3_5_B_C_D_E_H_L_iHL_A_Write8<N>(Fetch8());
	}

	template <int N> void DEC_0_3__B()
	{
		b4_5_BC_DE_HL_SP_Write16<N>(b4_5_BC_DE_HL_SP_Read16<N>() - 1);
	}

	template <int N> void JR_18()
	{
		PC += Fetch8();
	}

	template <int N> void LDI_2_2()
	{
		Write8(HL, A);
		++HL;
	}
	
	template <int N> void JR_2_3__0__2_3__8()
	{
		auto offset = Fetch8();
		if (b3_4_NZ_Z_NC_C_Eval<N>())
		{
			PC += offset;
		}
	}

	template <int N> void LDI_2_A()
	{
		A = Read8(HL);
		++HL;
	}

	template <int N> void LDD_3_2()
	{
		Write8(HL, A);
		--HL;
	}

	template <int N> void LDD_3_A()
	{
		A = Read8(HL);
		--HL;
	}

	template <int N> void OR_B__0_7()
	{
		OR(b0_2_B_C_D_E_H_L_iHL_A_Read8<N>());
	}

	template <int N> void ADD_C_6()
	{
		auto oldValue = A;
		auto operand = Fetch8();
		A = oldValue + operand;
		SetFlagsForAdd(oldValue, operand);
	}
	
	template <int N> void OR_F_6()
	{
		OR(Fetch8());
	}

	template <int N> void CP_F_E()
	{
		auto operand = Fetch8();
		SetFlagValue(FlagBitIndex::Zero, A == operand);
		SetFlagValue(FlagBitIndex::Subtract, true);
		// Note: documentation is flaky here; to verify
		SetFlagValue(FlagBitIndex::HalfCarry, GetLow4(A) < GetLow4(operand));
		SetFlagValue(FlagBitIndex::Carry, A < operand);
	}

	template <int N> void HALT_7_6()
	{
		m_cpuHalted = true;
	}
	
	///////////////////////////////////////////////////////////////////////////
	// CPU Emulation
	///////////////////////////////////////////////////////////////////////////

	void DebugOpcode(Uint8 opcode)
	{
		static const char* opcodeMnemonics[256] =
		{
			// This was preprocessed using macros and a spreadsheet from http://imrannazar.com/Gameboy-Z80-Opcode-Map
			"NOP", "LD BC,nn", "LD (BC),A", "INC BC", "INC B", "DEC B", "LD B,n", "RLC A", "LD (nn),SP", "ADD HL,BC", "LD A,(BC)", "DEC BC", "INC C", "DEC C", "LD C,n", "RRC A",
			"STOP", "LD DE,nn", "LD (DE),A", "INC DE", "INC D", "DEC D", "LD D,n", "RL A", "JR n", "ADD HL,DE", "LD A,(DE)", "DEC DE", "INC E", "DEC E", "LD E,n", "RR A",
			"JR NZ,n", "LD HL,nn", "LDI (HL),A", "INC HL", "INC H", "DEC H", "LD H,n", "DAA", "JR Z,n", "ADD HL,HL", "LDI A,(HL)", "DEC HL", "INC L", "DEC L", "LD L,n", "CPL",
			"JR NC,n", "LD SP,nn", "LDD (HL),A", "INC SP", "INC (HL)", "DEC (HL)", "LD (HL),n", "SCF", "JR C,n", "ADD HL,SP", "LDD A,(HL)", "DEC SP", "INC A", "DEC A", "LD A,n", "CCF",
			"LD B,B", "LD B,C", "LD B,D", "LD B,E", "LD B,H", "LD B,L", "LD B,(HL)", "LD B,A", "LD C,B", "LD C,C", "LD C,D", "LD C,E", "LD C,H", "LD C,L", "LD C,(HL)", "LD C,A",
			"LD D,B", "LD D,C", "LD D,D", "LD D,E", "LD D,H", "LD D,L", "LD D,(HL)", "LD D,A", "LD E,B", "LD E,C", "LD E,D", "LD E,E", "LD E,H", "LD E,L", "LD E,(HL)", "LD E,A",
			"LD H,B", "LD H,C", "LD H,D", "LD H,E", "LD H,H", "LD H,L", "LD H,(HL)", "LD H,A", "LD L,B", "LD L,C", "LD L,D", "LD L,E", "LD L,H", "LD L,L", "LD L,(HL)", "LD L,A",
			"LD (HL),B", "LD (HL),C", "LD (HL),D", "LD (HL),E", "LD (HL),H", "LD (HL),L", "HALT", "LD (HL),A", "LD A,B", "LD A,C", "LD A,D", "LD A,E", "LD A,H", "LD A,L", "LD A,(HL)", "LD A,A",
			"ADD A,B", "ADD A,C", "ADD A,D", "ADD A,E", "ADD A,H", "ADD A,L", "ADD A,(HL)", "ADD A,A", "ADC A,B", "ADC A,C", "ADC A,D", "ADC A,E", "ADC A,H", "ADC A,L", "ADC A,(HL)", "ADC A,A",
			"SUB A,B", "SUB A,C", "SUB A,D", "SUB A,E", "SUB A,H", "SUB A,L", "SUB A,(HL)", "SUB A,A", "SBC A,B", "SBC A,C", "SBC A,D", "SBC A,E", "SBC A,H", "SBC A,L", "SBC A,(HL)", "SBC A,A",
			"AND B", "AND C", "AND D", "AND E", "AND H", "AND L", "AND (HL)", "AND A", "XOR B", "XOR C", "XOR D", "XOR E", "XOR H", "XOR L", "XOR (HL)", "XOR A",
			"OR B", "OR C", "OR D", "OR E", "OR H", "OR L", "OR (HL)", "OR A", "CP B", "CP C", "CP D", "CP E", "CP H", "CP L", "CP (HL)", "CP A",
			"RET NZ", "POP BC", "JP NZ,nn", "JP nn", "CALL NZ,nn", "PUSH BC", "ADD A,n", "RST 0", "RET Z", "RET", "JP Z,nn", "Ext ops", "CALL Z,nn", "CALL nn", "ADC A,n", "RST 8",
			"RET NC", "POP DE", "JP NC,nn", "XX", "CALL NC,nn", "PUSH DE", "SUB A,n", "RST 10", "RET C", "RETI", "JP C,nn", "XX", "CALL C,nn", "XX", "SBC A,n", "RST 18",
			"LDH (n),A", "POP HL", "LDH (C),A", "XX", "XX", "PUSH HL", "AND n", "RST 20", "ADD SP,d", "JP (HL)", "LD (nn),A", "XX", "XX", "XX", "XOR n", "RST 28",
			"LDH A,(n)", "POP AF", "XX", "DI", "XX", "PUSH AF", "OR n", "RST 30", "LDHL SP,d", "LD SP,HL", "LD A,(nn)", "EI", "XX", "XX", "CP n", "RST 38",
		};

		printf("0x%04lX: %s  (0x%02lX)\n", PC, opcodeMnemonics[opcode], opcode);
		printf("A: 0x%02lX F: %s%s%s%s B: 0x%02lX C: 0x%02lX D: 0x%02lX E: 0x%02lX H: 0x%02lX L: 0x%02lX\n",
			A,
			GetFlagValue(FlagBitIndex::Zero) ? "Z" : "z",
			GetFlagValue(FlagBitIndex::Subtract) ? "S" : "s",
			GetFlagValue(FlagBitIndex::HalfCarry) ? "H" : "h",
			GetFlagValue(FlagBitIndex::Carry) ? "C" : "c", 
			B, C, D, E, H, L);
		printf("AF: 0x%04lX BC: 0x%04lX DE: 0x%04lX HL: 0x%04lX SP: 0x%04lX IME: %d\n", AF, BC, DE, HL, SP, IME ? 1 : 0);
		printf("n: 0x%s nn: 0x%s\n", DebugStringPeek8(PC + 1).c_str(), DebugStringPeek16(PC + 1).c_str());
		//printf("(BC): 0x%s (DE): 0x%s (HL): 0x%s (nn): 0x%s\n", DebugStringPeek8(BC), DebugStringPeek8(DE), DebugStringPeek8(HL), DebugStringPeek16(Peek16(PC + 1)));
		printf("(BC): 0x%s (DE): 0x%s (HL): 0x%s\n", DebugStringPeek8(BC).c_str(), DebugStringPeek8(DE).c_str(), DebugStringPeek8(HL).c_str());
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
		// This requires a case label per opcode, but it generates debuggable code in debug targets and very efficient code in release.  (Many LD variants compile to two MOV instructions.)
		
		while (m_cyclesRemaining > 0)
		{
			DebugOpcode(Peek8());
			Uint8 opcode = Fetch8();

			Sint32 instructionCycles = -1; // number of clock cycles used by the opcode

#define OPCODE(code, cycles, name) case code: instructionCycles = (cycles); name<code>(); break;
			switch (opcode)
			{
			OPCODE(0x00, 4, NOP)
			
			OPCODE(0x0A, 8, LD_0_1__A)
			OPCODE(0x1A, 8, LD_0_1__A)

			OPCODE(0x01, 12, LD_0_3__1)
			OPCODE(0x11, 12, LD_0_3__1)
			OPCODE(0x21, 12, LD_0_3__1)
			OPCODE(0x31, 12, LD_0_3__1)

			OPCODE(0x03, 8, INC_0_3__3)
			OPCODE(0x13, 8, INC_0_3__3)
			OPCODE(0x23, 8, INC_0_3__3)
			OPCODE(0x33, 8, INC_0_3__3)

			OPCODE(0x04, 4, INC_0_3__4__0_3__C)
			OPCODE(0x0C, 4, INC_0_3__4__0_3__C)
			OPCODE(0x14, 4, INC_0_3__4__0_3__C)
			OPCODE(0x1C, 4, INC_0_3__4__0_3__C)
			OPCODE(0x24, 4, INC_0_3__4__0_3__C)
			OPCODE(0x2C, 4, INC_0_3__4__0_3__C)
			OPCODE(0x34, 8, INC_0_3__4__0_3__C)
			OPCODE(0x3C, 4, INC_0_3__4__0_3__C)

			OPCODE(0x05, 4, DEC_0_3__5__0_3__D)
			OPCODE(0x0D, 4, DEC_0_3__5__0_3__D)
			OPCODE(0x15, 4, DEC_0_3__5__0_3__D)
			OPCODE(0x1D, 4, DEC_0_3__5__0_3__D)
			OPCODE(0x25, 4, DEC_0_3__5__0_3__D)
			OPCODE(0x2D, 4, DEC_0_3__5__0_3__D)
			OPCODE(0x35, 8, DEC_0_3__5__0_3__D)
			OPCODE(0x3D, 4, DEC_0_3__5__0_3__D)

			OPCODE(0x06, 8, LD_0_3__6__0_3__E)
			OPCODE(0x0E, 8, LD_0_3__6__0_3__E)
			OPCODE(0x16, 8, LD_0_3__6__0_3__E)
			OPCODE(0x1E, 8, LD_0_3__6__0_3__E)
			OPCODE(0x26, 8, LD_0_3__6__0_3__E)
			OPCODE(0x2E, 8, LD_0_3__6__0_3__E)
			OPCODE(0x36, 12, LD_0_3__6__0_3__E)
			OPCODE(0x3E, 8, LD_0_3__6__0_3__E)
			
			OPCODE(0x0B, 8, DEC_0_3__B)
			OPCODE(0x1B, 8, DEC_0_3__B)
			OPCODE(0x2B, 8, DEC_0_3__B)
			OPCODE(0x3B, 8, DEC_0_3__B)

			OPCODE(0x18, 8, JR_18)

			OPCODE(0x22, 8, LDI_2_2)
			OPCODE(0x32, 8, LDD_3_2)
			OPCODE(0x2A, 8, LDI_2_A)
			OPCODE(0x3A, 8, LDD_3_A)

			OPCODE(0x20, 8, JR_2_3__0__2_3__8)
			OPCODE(0x28, 8, JR_2_3__0__2_3__8)
			OPCODE(0x30, 8, JR_2_3__0__2_3__8)
			OPCODE(0x38, 8, JR_2_3__0__2_3__8)

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
			OPCODE(0x76, 4, HALT_7_6);

			// OR ?
			OPCODE(0xB0, 4, OR_B__0_7)
			OPCODE(0xB1, 4, OR_B__0_7)
			OPCODE(0xB2, 4, OR_B__0_7)
			OPCODE(0xB3, 4, OR_B__0_7)
			OPCODE(0xB4, 4, OR_B__0_7)
			OPCODE(0xB5, 4, OR_B__0_7)
			OPCODE(0xB6, 8, OR_B__0_7)
			OPCODE(0xB7, 4, OR_B__0_7)
			OPCODE(0xF6, 8, OR_F_6)

			OPCODE(0xC6, 8, ADD_C_6)
			
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

			OPCODE(0xFE, 8, CP_F_E);

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

			if (m_debuggerState == DebuggerState::SingleStepping)
			{
				auto keycode = WaitForKeypress();
				switch (keycode)
				{
				case SDLK_g: m_debuggerState = DebuggerState::Running; break;
				case SDLK_s: m_debuggerState = DebuggerState::SingleStepping; break;
				}
			}
		}
	}

private:
	///////////////////////////////////////////////////////////////////////////
	// Memory access
	///////////////////////////////////////////////////////////////////////////

	std::string DebugStringPeek8(Uint16 address)
	{
		Uint8 value = 0;
		bool success = m_pMemory->SafeRead8(address, value);
		char szBuffer[32];
		_snprintf_s(szBuffer, ARRAY_SIZE(szBuffer), "%02lX", value);
		return success ? szBuffer : "??";
	}
	
	std::string DebugStringPeek16(Uint16 address)
	{
		return DebugStringPeek8(address + 1).append(DebugStringPeek8(address));
	}

	Uint8 Peek8()
	{
		return m_pMemory->Read8(PC);
	}

	Uint8 Peek8(Uint16 address)
	{
		return m_pMemory->Read8(address);
	}

	Uint16 Peek16()
	{
		return m_pMemory->Read16(PC);
	}

	Uint16 Peek16(Uint16 address)
	{
		return m_pMemory->Read16(address);
	}

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

	///////////////////////////////////////////////////////////////////////////
	// Flags
	///////////////////////////////////////////////////////////////////////////

	void SetFlagsForAdd(Uint8 oldValue, Uint8 operand, Uint8 flagMask = FlagBitMask::All)
	{
		if (flagMask & FlagBitMask::Zero)
		{
			SetZeroFromValue(A);
		}

		if (flagMask & FlagBitMask::Subtract)
		{
			SetFlagValue(FlagBitIndex::Subtract, false);
		}

		if (flagMask & FlagBitMask::HalfCarry)
		{
			SetFlagValue(FlagBitIndex::HalfCarry, (static_cast<Uint16>(GetLow4(oldValue)) + GetLow4(operand)) > 0xF);
		}

		if (flagMask & FlagBitMask::Carry)
		{
			SetFlagValue(FlagBitIndex::Carry, (static_cast<Uint16>(oldValue) + operand) > 0xFF);
		}
	}

	void SetFlagsForSub(Uint8 oldValue, Uint8 operand, Uint8 flagMask = FlagBitMask::All)
	{
		if (flagMask & FlagBitMask::Zero)
		{
			SetZeroFromValue(A);
		}

		if (flagMask & FlagBitMask::Subtract)
		{
			SetFlagValue(FlagBitIndex::Subtract, true);
		}

		if (flagMask & FlagBitMask::HalfCarry)
		{
			SetFlagValue(FlagBitIndex::HalfCarry, static_cast<Uint16>(GetLow4(oldValue)) >= GetLow4(operand));
		}

		if (flagMask & FlagBitMask::Carry)
		{
			SetFlagValue(FlagBitIndex::Carry, static_cast<Uint16>(oldValue) >= operand);
		}
	}

	void SetZeroFromValue(Uint8 value)
	{
		SetFlagValue(FlagBitIndex::Zero, value == 0);
	}

	void SetFlagValue(FlagBitIndex position, bool value)
	{
		auto bitMask = (1 << static_cast<Uint8>(position));
		F = value ? (F | bitMask) : (F & ~bitMask);
	}

	bool GetFlagValue(FlagBitIndex position)
	{
		return (F & (1 << static_cast<Uint8>(position))) != 0;
	}

	///////////////////////////////////////////////////////////////////////////
	// CPU State members
	///////////////////////////////////////////////////////////////////////////

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
	bool m_cpuHalted;

	float m_cyclesRemaining;
	Uint32 m_totalOpcodesExecuted;

	DebuggerState m_debuggerState;

	std::shared_ptr<Memory> m_pMemory;
};
