#pragma once

#include "MemoryBus.h"

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

	Cpu(const std::shared_ptr<MemoryBus>& memory)
		: m_pMemory(memory)
		, IE(memory->IE)
		, IF(memory->IF)
	{
		Reset();
	}

	void Reset()
	{
		m_totalOpcodesExecuted = 0;
		m_traceEnabled = false;

		m_cpuHalted = false;
		m_cpuStopped = false;

		IME = true;

		static_assert(offsetof(Cpu, F) == offsetof(Cpu, AF), "Target machine is not little-endian; register unions must be revised");
		PC = 0x0100;
		AF = 0x01B0;
		BC = 0x0013;
		DE = 0x00D8;
		HL = 0x014D;
	}

	void DebugNextOpcode()
	{
		DebugOpcode(Peek8());
	}

	Sint32 ExecuteSingleInstruction()
	{
		Sint32 instructionCycles = -1; // number of clock cycles used by the opcode

		// Check for interrupts
		if (IME)
		{
			if (CallInterruptVectorIfRequired(Bit0, 0x40))
			{
				// VBlank interrupt
			}
			else if (CallInterruptVectorIfRequired(Bit1, 0x48))
			{
				// LCD STAT
			}
			else if (CallInterruptVectorIfRequired(Bit2, 0x50))
			{
				// Timer
			}
			else if (CallInterruptVectorIfRequired(Bit3, 0x58))
			{
				// Serial
			}
			else if (CallInterruptVectorIfRequired(Bit4, 0x60))
			{
				// Joypad
			}
		}

		if (m_cpuHalted && IF)
		{
			m_cpuHalted = false;
		}

		if (!m_cpuHalted && !m_cpuStopped)
		{
			instructionCycles = DoExecuteSingleInstruction();
		}
		else
		{
			// Simply wait until something interesting occurs, depending on the CPU state
			//@TODO: handle HALT
			//@TODO: handle STOP
			instructionCycles = 4;
		}

		SDL_assert(instructionCycles != -1);
		return instructionCycles;
	}

	void SetTraceEnabled(bool enabled)
	{
		m_traceEnabled = enabled;
	}

private:
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

	// B_C_D_E_H_L_iHL_A
	template <int N> Uint8& B_C_D_E_H_L_iHL_A_GetReg8();
	template <> Uint8& B_C_D_E_H_L_iHL_A_GetReg8<0>() { return B; }
	template <> Uint8& B_C_D_E_H_L_iHL_A_GetReg8<1>() { return C; }
	template <> Uint8& B_C_D_E_H_L_iHL_A_GetReg8<2>() { return D; }
	template <> Uint8& B_C_D_E_H_L_iHL_A_GetReg8<3>() { return E; }
	template <> Uint8& B_C_D_E_H_L_iHL_A_GetReg8<4>() { return H; }
	template <> Uint8& B_C_D_E_H_L_iHL_A_GetReg8<5>() { return L; }
	template <> Uint8& B_C_D_E_H_L_iHL_A_GetReg8<7>() { return A; }
	
	template <int N> Uint16 B_C_D_E_H_L_iHL_A_GetAddress();
	template <> Uint16 B_C_D_E_H_L_iHL_A_GetAddress<6>() { return HL; }

	template <int N> Uint8 B_C_D_E_H_L_iHL_A_Read8() { return B_C_D_E_H_L_iHL_A_GetReg8<N>(); }
	template <> Uint8 B_C_D_E_H_L_iHL_A_Read8<6>() { return Read8(B_C_D_E_H_L_iHL_A_GetAddress<6>()); }
	template <int N> void B_C_D_E_H_L_iHL_A_Write8(Uint8 value) { B_C_D_E_H_L_iHL_A_GetReg8<N>() = value; }
	template <> void B_C_D_E_H_L_iHL_A_Write8<6>(Uint8 value) { Write8(B_C_D_E_H_L_iHL_A_GetAddress<6>(), value); }

	// NZ_Z_NC_C_Eval
	template <int N> bool NZ_Z_NC_C_Eval();
	template <> bool NZ_Z_NC_C_Eval<0>() { return !GetFlagValue(FlagBitIndex::Zero); } 
	template <> bool NZ_Z_NC_C_Eval<1>() { return GetFlagValue(FlagBitIndex::Zero); } 
	template <> bool NZ_Z_NC_C_Eval<2>() { return !GetFlagValue(FlagBitIndex::Carry); } 
	template <> bool NZ_Z_NC_C_Eval<3>() { return GetFlagValue(FlagBitIndex::Carry); } 
	
	// iBC_iDE
	template <int N> Uint16 iBC_iDE_GetAddress();
	template <> Uint16 iBC_iDE_GetAddress<0>() { return BC; }
	template <> Uint16 iBC_iDE_GetAddress<1>() { return DE; }
	template <int N> Uint8 iBC_iDE_Read8() { return Read8(iBC_iDE_GetAddress<N>()); }
	template <int N> void iBC_iDE_Write8(Uint8 value) { Write8(iBC_iDE_GetAddress<N>(), value); }

	// BC_DE_HL_SP
	template <int N> Uint16& BC_DE_HL_SP_GetReg16();
	template <> Uint16& BC_DE_HL_SP_GetReg16<0>() { return BC; }
	template <> Uint16& BC_DE_HL_SP_GetReg16<1>() { return DE; }
	template <> Uint16& BC_DE_HL_SP_GetReg16<2>() { return HL; }
	template <> Uint16& BC_DE_HL_SP_GetReg16<3>() { return SP; }
	template <int N> Uint16 BC_DE_HL_SP_Read16() { return BC_DE_HL_SP_GetReg16<N>(); }
	template <int N> void BC_DE_HL_SP_Write16(Uint16 value) { BC_DE_HL_SP_GetReg16<N>() = value; }

	// Bindings for the above to specific bits in the opcode
	template <int N> Uint8 b0_2_B_C_D_E_H_L_iHL_A_Read8() { return B_C_D_E_H_L_iHL_A_Read8<b0_2<N>::Value>(); }
	template <int N> void b0_2_B_C_D_E_H_L_iHL_A_Write8(Uint8 value) { B_C_D_E_H_L_iHL_A_Write8<b0_2<N>::Value>(value); }

	template <int N> bool b3_4_NZ_Z_NC_C_Eval() { return NZ_Z_NC_C_Eval<b3_4<N>::Value>(); }

	template <int N> Uint8 b3_5_B_C_D_E_H_L_iHL_A_Read8() { return B_C_D_E_H_L_iHL_A_Read8<b3_5<N>::Value>(); }
	template <int N> void b3_5_B_C_D_E_H_L_iHL_A_Write8(Uint8 value) { B_C_D_E_H_L_iHL_A_Write8<b3_5<N>::Value>(value); }

	template <int N> Uint8 b4_iBC_iDE_Read8() { return iBC_iDE_Read8<b4<N>::Value>(); }
	template <int N> void b4_iBC_iDE_Write8(Uint8 value) { iBC_iDE_Write8<b4<N>::Value>(value); }

	template <int N> Uint16 b4_5_BC_DE_HL_SP_Read16() { return BC_DE_HL_SP_Read16<b4_5<N>::Value>(); }
	template <int N> void b4_5_BC_DE_HL_SP_Write16(Uint16 value) { BC_DE_HL_SP_Write16<b4_5<N>::Value>(value); }

	///////////////////////////////////////////////////////////////////////////
	// Opcode implementations
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

	void ADD(Uint8 operand, Uint8 carry = 0)
	{
		auto oldValue = A;
		A = oldValue + operand + carry;
		SetFlagsForAdd(oldValue, operand, carry, FlagBitMask::All);
	}

	void ADC(Uint8 operand)
	{
		ADD(operand, GetFlagValue(FlagBitIndex::Carry) ? 1 : 0);
	}

	void SUB(Uint8 operand, Uint8 carry = 0)
	{
		auto oldValue = A;
		A = oldValue - (operand + carry);
		SetFlagsForSub(oldValue, operand, carry, FlagBitMask::All);
	}

	void SBC(Uint8 operand)
	{
		SUB(operand, GetFlagValue(FlagBitIndex::Carry) ? 1 : 0);
	}

	void AND(Uint8 value)
	{
		A &= value;
		SetZeroFlagFromValue(A);
		SetFlagValue(FlagBitIndex::Subtract, false);
		SetFlagValue(FlagBitIndex::HalfCarry, true);
		SetFlagValue(FlagBitIndex::Carry, false);
	}
	
	void OR(Uint8 value)
	{
		A |= value;
		SetZeroFlagFromValue(A);
		SetFlagValue(FlagBitIndex::Subtract, false);
		SetFlagValue(FlagBitIndex::HalfCarry, false);
		SetFlagValue(FlagBitIndex::Carry, false);
	}

	void XOR(Uint8 value)
	{
		A ^= value;
		SetZeroFlagFromValue(A);
		SetFlagValue(FlagBitIndex::Subtract, false);
		SetFlagValue(FlagBitIndex::HalfCarry, false);
		SetFlagValue(FlagBitIndex::Carry, false);
	}

	Uint8 RLC(Uint8 oldValue, bool setZeroFlagFromValue)
	{

		Uint8 newValue = (oldValue << 1) | ((oldValue & Bit7) >> 7);
		if (setZeroFlagFromValue)
		{
			SetZeroFlagFromValue(newValue);
		}
		else
		{
			SetFlagValue(FlagBitIndex::Zero, false);
		}
		SetFlagValue(FlagBitIndex::Subtract, false);
		SetFlagValue(FlagBitIndex::HalfCarry, false);
		SetFlagValue(FlagBitIndex::Carry, (oldValue & Bit7) != 0);
		return newValue;
	}

	Uint8 RRC(Uint8 oldValue, bool setZeroFlagFromValue)
	{
		Uint8 newValue = (oldValue >> 1) | ((oldValue & Bit0) << 7);
		if (setZeroFlagFromValue)
		{
			SetZeroFlagFromValue(newValue);
		}
		else
		{
			SetFlagValue(FlagBitIndex::Zero, false);
		}
		SetFlagValue(FlagBitIndex::Subtract, false);
		SetFlagValue(FlagBitIndex::HalfCarry, false);
		SetFlagValue(FlagBitIndex::Carry, (oldValue & Bit0) != 0);
		return newValue;
	}

	Uint8 RL(Uint8 oldValue, bool setZeroFlagFromValue)
	{
		Uint8 newValue = (oldValue << 1) | (GetFlagValue(FlagBitIndex::Carry) ? Bit0 : 0);
		if (setZeroFlagFromValue)
		{
			SetZeroFlagFromValue(newValue);
		}
		else
		{
			SetFlagValue(FlagBitIndex::Zero, false);
		}
		SetFlagValue(FlagBitIndex::Subtract, false);
		SetFlagValue(FlagBitIndex::HalfCarry, false);
		SetFlagValue(FlagBitIndex::Carry, (oldValue & Bit7) != 0);
		return newValue;
	}

	Uint8 RR(Uint8 oldValue, bool setZeroFlagFromValue)
	{
		Uint8 newValue = (oldValue >> 1) | (GetFlagValue(FlagBitIndex::Carry) ? Bit7 : 0);
		if (setZeroFlagFromValue)
		{
			SetZeroFlagFromValue(newValue);
		}
		else
		{
			SetFlagValue(FlagBitIndex::Zero, false);
		}
		SetFlagValue(FlagBitIndex::Subtract, false);
		SetFlagValue(FlagBitIndex::HalfCarry, false);
		SetFlagValue(FlagBitIndex::Carry, (oldValue & Bit0) != 0);
		return newValue;
	}

	void CP(Uint8 operand)
	{
		SetFlagValue(FlagBitIndex::Zero, A == operand);
		SetFlagValue(FlagBitIndex::Subtract, true);
		// Note: documentation is flaky here; to verify
		SetFlagValue(FlagBitIndex::HalfCarry, GetLow4(A) < GetLow4(operand));
		SetFlagValue(FlagBitIndex::Carry, A < operand);
	}

	void Call(Uint16 address)
	{
		Push16(PC);
		PC = address;
	}

	void CallI(Uint16 address)
	{
		IME = false;
		Call(address);
	}

	void Ret()
	{
		PC = Pop16();
	}

	template <int N> void NOP_0__0()
	{
	}

	template <int N> void LD_0_1__2()
	{
		b4_iBC_iDE_Write8<N>(A);
	}

	template <int N> void LD_0_1__A()
	{
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
		SetFlagsForAdd(oldValue, 1, 0, FlagBitMask::Zero | FlagBitMask::Subtract | FlagBitMask::HalfCarry);
	}

	template <int N> void DEC_0_3__5__0_3__D()
	{
		auto oldValue = b3_5_B_C_D_E_H_L_iHL_A_Read8<N>();
		auto newValue = oldValue - 1;
		b3_5_B_C_D_E_H_L_iHL_A_Write8<N>(newValue);
		SetFlagsForSub(oldValue, 1, 0, FlagBitMask::Zero | FlagBitMask::Subtract | FlagBitMask::HalfCarry);
	}

	template <int N> void LD_0_3__6__0_3__E()
	{
		b3_5_B_C_D_E_H_L_iHL_A_Write8<N>(Fetch8());
	}

	template <int N> void DEC_0_3__B()
	{
		b4_5_BC_DE_HL_SP_Write16<N>(b4_5_BC_DE_HL_SP_Read16<N>() - 1);
	}

	template <int N> void ADD_0_3__9()
	{
		auto oldValue = HL;
		auto operand = b4_5_BC_DE_HL_SP_Read16<N>();
		HL += operand;
		SetFlagsForAdd16(oldValue, operand);
	}

	template <int N> void RLC_0__7()
	{
		A = RLC(A, false);
	}

	template <int N> void LD_0__8()
	{
		Write16(Fetch16(), SP);
	}

	template <int N> void RRC_0__F()
	{
		A = RRC(A, false);
	}

	template <int N> void STOP_1__0()
	{
		m_cpuStopped = true;
		printf("CPU STOPped");
	}

	template <int N> void RL_1__7()
	{
		A = RL(A, false);
	}

	template <int N> void JR_1__8()
	{
		Sint8 displacement = static_cast<Sint8>(Fetch8()); // the offset can be negative here
		PC += displacement;
	}

	template <int N> void RR_1__F()
	{
		A = RR(A, false);
	}

	template <int N> void JR_2_3__0__2_3__8()
	{
		Sint8 displacement = static_cast<Sint8>(Fetch8()); // the offset can be negative here
		if (b3_4_NZ_Z_NC_C_Eval<N>())
		{
			PC += displacement;
		}
	}

	template <int N> void LDI_2__2()
	{
		Write8(HL, A);
		++HL;
	}
	
	template <int N> void LDI_2__A()
	{
		A = Read8(HL);
		++HL;
	}

	template <int N> void LDD_3__2()
	{
		Write8(HL, A);
		--HL;
	}

	template <int N> void LDD_3__A()
	{
		A = Read8(HL);
		--HL;
	}

	template <int N> void DAA_2__7()
	{
		// Algorithm adapted from DKParrot's post on http://ngemu.com/threads/little-help-with-my-gameboy-emulator.143814/
		// Seems to be relatively little documentation regarding this peculiar opcode other than other emulators' source.
		// Since getting this right with all the edge cases from first principles looks like an exercise in pedanticism
		// and the implementation of this specific opcode is not of particular interest to me, I chose to adapt an existing
		// implementation and move on with my life.
		Sint32 value = A;

        if (!GetFlagValue(FlagBitIndex::Subtract))
        {
            if (GetFlagValue(FlagBitIndex::HalfCarry) || ((value & 0xF) > 9))
			{
                value += 0x06;
			}

            if (GetFlagValue(FlagBitIndex::Carry) || (value > 0x9F))
			{
                value += 0x60;
			}
        }
        else
        {
            if (GetFlagValue(FlagBitIndex::HalfCarry))
			{
                value = (value - 6) & 0xFF;
			}

            if (GetFlagValue(FlagBitIndex::Carry))
			{
                value -= 0x60;
			}
        }

		SetFlagValue(FlagBitIndex::HalfCarry, false);

		// Very strange behaviour on the carry flag - if there is a carry, it is set, otherwise it is untouched.
		// Not sure this is truly how it behaves on a real DMG.
		if (value & 0x100)
		{
			SetFlagValue(FlagBitIndex::Carry, true);
		}
		//SetFlagValue(FlagBitIndex::Carry, (value & 0x100) != 0);

		A = static_cast<Uint8>(value);
		SetZeroFlagFromValue(A);
	}

	template <int N> void CPL_2__F()
	{
		A = ~A;
		SetFlagValue(FlagBitIndex::Subtract, true);
		SetFlagValue(FlagBitIndex::HalfCarry, true);
	}

	template <int N> void SCF_3__7()
	{
		SetFlagValue(FlagBitIndex::Subtract, false);
		SetFlagValue(FlagBitIndex::HalfCarry, false);
		SetFlagValue(FlagBitIndex::Carry, true);
	}

	template <int N> void CCF_3__F()
	{
		SetFlagValue(FlagBitIndex::Subtract, false);
		SetFlagValue(FlagBitIndex::HalfCarry, false);
		SetFlagValue(FlagBitIndex::Carry, !GetFlagValue(FlagBitIndex::Carry));
	}

	template <int N> void LD_4_7__0_F__NO_7__6()
	{
		b3_5_B_C_D_E_H_L_iHL_A_Write8<N>(b0_2_B_C_D_E_H_L_iHL_A_Read8<N>());
	}
	
	template <int N> void HALT_7__6()
	{
		m_cpuHalted = true;
	}

	template <int N> void ADD_8__0_7()
	{
		ADD(b0_2_B_C_D_E_H_L_iHL_A_Read8<N>());
	}
	
	template <int N> void ADC_8__8_F()
	{
		ADC(b0_2_B_C_D_E_H_L_iHL_A_Read8<N>());
	}

	template <int N> void SUB_9__0_7()
	{
		SUB(b0_2_B_C_D_E_H_L_iHL_A_Read8<N>());
	}
	
	template <int N> void SBC_9__8_F()
	{
		SBC(b0_2_B_C_D_E_H_L_iHL_A_Read8<N>());
	}

	template <int N> void AND_A__0_7()
	{
		AND(b0_2_B_C_D_E_H_L_iHL_A_Read8<N>());
	}
	
	template <int N> void XOR_A__8_F()
	{
		XOR(b0_2_B_C_D_E_H_L_iHL_A_Read8<N>());
	}

	template <int N> void OR_B__0_7()
	{
		OR(b0_2_B_C_D_E_H_L_iHL_A_Read8<N>());
	}

	template <int N> void CP_B__8_F()
	{
		CP(b0_2_B_C_D_E_H_L_iHL_A_Read8<N>());
	}

	template <int N> void RET_C_D__0__C_D__8()
	{
		if (b3_4_NZ_Z_NC_C_Eval<N>())
		{
			Ret();
		}
	}

	template <int N> void JP_C_D__2__C_D__2()
	{
		auto address = Fetch16();
		if (b3_4_NZ_Z_NC_C_Eval<N>())
		{
			PC = address;
		}
	}

	template <int N> void CALL_C_D__4__C_D__C()
	{
		auto address = Fetch16();
		if (b3_4_NZ_Z_NC_C_Eval<N>())
		{
			Call(address);
		}
	}

	template <int N> void ADD_C_6()
	{
		ADD(Fetch8());
	}

	template <int N> void RST_C_F__7__C_F__F()
	{
		auto isrAddress = b3_5<N>::Value * 8;
		Call(isrAddress);
	}

	template <int N> void RLC_CB_0__0_7()
	{
		b0_2_B_C_D_E_H_L_iHL_A_Write8<N>(RLC(b0_2_B_C_D_E_H_L_iHL_A_Read8<N>(), true));
	}

	template <int N> void RRC_CB_0__8_F()
	{
		b0_2_B_C_D_E_H_L_iHL_A_Write8<N>(RRC(b0_2_B_C_D_E_H_L_iHL_A_Read8<N>(), true));
	}

	template <int N> void RL_CB_1__0_7()
	{
		b0_2_B_C_D_E_H_L_iHL_A_Write8<N>(RL(b0_2_B_C_D_E_H_L_iHL_A_Read8<N>(), true));
	}
	
	template <int N> void RR_CB_1__8_F()
	{
		b0_2_B_C_D_E_H_L_iHL_A_Write8<N>(RR(b0_2_B_C_D_E_H_L_iHL_A_Read8<N>(), true));
	}

	template <int N> void SLA_CB_2__0_7()
	{
		Uint8 oldValue = b0_2_B_C_D_E_H_L_iHL_A_Read8<N>();
		Uint8 newValue = oldValue << 1;
		b0_2_B_C_D_E_H_L_iHL_A_Write8<N>(newValue);
		SetZeroFlagFromValue(newValue);
		SetFlagValue(FlagBitIndex::Subtract, false);
		SetFlagValue(FlagBitIndex::HalfCarry, false);
		SetFlagValue(FlagBitIndex::Carry, (oldValue & Bit7) != 0);
	}
	
	template <int N> void SRA_CB_2__8_F()
	{
		Uint8 oldValue = b0_2_B_C_D_E_H_L_iHL_A_Read8<N>();
		Uint8 newValue = (oldValue >> 1) | (oldValue & Bit7);
		b0_2_B_C_D_E_H_L_iHL_A_Write8<N>(newValue);
		SetZeroFlagFromValue(newValue);
		SetFlagValue(FlagBitIndex::Subtract, false);
		SetFlagValue(FlagBitIndex::HalfCarry, false);
		SetFlagValue(FlagBitIndex::Carry, (oldValue & Bit0) != 0);
	}

	template <int N> void SWAP_CB_3__0_7()
	{
		auto oldValue = b0_2_B_C_D_E_H_L_iHL_A_Read8<N>();
		Uint8 newValue = GetHigh4(oldValue) | (GetLow4(oldValue) << 4);
		b0_2_B_C_D_E_H_L_iHL_A_Write8<N>(newValue);
		SetZeroFlagFromValue(newValue);
		SetFlagValue(FlagBitIndex::Subtract, false);
		SetFlagValue(FlagBitIndex::HalfCarry, false);
		SetFlagValue(FlagBitIndex::Carry, false);
	}

	template <int N> void SRL_CB_3__8_F()
	{
		Uint8 oldValue = b0_2_B_C_D_E_H_L_iHL_A_Read8<N>();
		Uint8 newValue = oldValue >> 1;
		b0_2_B_C_D_E_H_L_iHL_A_Write8<N>(newValue);
		SetZeroFlagFromValue(newValue);
		SetFlagValue(FlagBitIndex::Subtract, false);
		SetFlagValue(FlagBitIndex::HalfCarry, false);
		SetFlagValue(FlagBitIndex::Carry, (oldValue & Bit0) != 0);
	}

	template <int N> void ADC_C__E()
	{
		ADC(Fetch8());
	}

	template <int N> void SUB_D__6()
	{
		SUB(Fetch8());
	}

	template <int N> void RETI_D__9()
	{
		Ret();
		IME = true;
	}

	template <int N> void SBC_D__E()
	{
		SBC(Fetch8());
	}

	template <int N> void AND_E__6()
	{
		AND(Fetch8());
	}

	template <int N> void JP_E__9()
	{
		// Bizarre docs: this is listed as JP (HL), but I'm not sure why there is a dereference around HL since the timing and docs both imply it's just PC = HL
		PC = HL;
	}

	template <int N> void XOR_E__E()
	{
		XOR(Fetch8());
	}

	template <int N> void OR_F_6()
	{
		OR(Fetch8());
	}

	template <int N> void ADD_E__8()
	{
		Sint8 displacement = Fetch8();
		auto oldValue = SP;
		SP = SP + displacement;
		SetFlagValue(FlagBitIndex::Zero, false);
		SetFlagValue(FlagBitIndex::Subtract, false);

		SetFlagsForAdd8To16(oldValue, displacement);
	}

	template <int N> void LDH_F__0()
	{
		auto displacement = Fetch8();
		auto address = displacement + 0xFF00;
		A =	 Read8(address);
	}

	template <int N> void DI_F__3()
	{
		IME = false;
	}

	template <int N> void LDHL_F__8()
	{
		auto oldValue = SP;
		Sint8 displacement = Fetch8();
		HL = SP + displacement;
		SetFlagValue(FlagBitIndex::Zero, false);
		SetFlagValue(FlagBitIndex::Subtract, false);

		SetFlagsForAdd8To16(oldValue, displacement);
	}

	template <int N> void LD_F__9()
	{
		SP = HL;
	}

	template <int N> void LD_F__A()
	{
		auto address = Fetch16();
		A = Read8(address);
	}

	template <int N> void EI_F__B()
	{
		IME = true;
	}

	template <int N> void CP_F_E()
	{
		CP(Fetch8());
	}

	template <int N> void IllegalOpcode()
	{
		throw Exception("Illegal opcode executed: 0x%02lX", N);
	}
	
	///////////////////////////////////////////////////////////////////////////
	// CPU Emulation
	///////////////////////////////////////////////////////////////////////////

	void DebugOpcode(Uint8 opcode)
	{
		if (m_traceEnabled)
		{
			//SetForegroundConsoleColor();

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

			printf("-----\n0x%04lX: %s  (0x%02lX)\n", PC, opcodeMnemonics[opcode], opcode);
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
	}

	Uint16 DoExecuteSingleInstruction()
	{
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

		Uint8 opcode = Fetch8();
		bool unknownOpcode = false;

		Sint32 instructionCycles = -1; // number of clock cycles used by the opcode

#define OPCODE(code, cycles, name) case code: instructionCycles = (cycles); name<code>(); break;
		switch (opcode)
		{
		OPCODE(0x00, 4, NOP_0__0)
			
		OPCODE(0x02, 8, LD_0_1__2)
		OPCODE(0x12, 8, LD_0_1__2)

		OPCODE(0x08, 20, LD_0__8)

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

		OPCODE(0x07, 4, RLC_0__7)

		OPCODE(0x09, 8, ADD_0_3__9)
		OPCODE(0x19, 8, ADD_0_3__9)
		OPCODE(0x29, 8, ADD_0_3__9)
		OPCODE(0x39, 8, ADD_0_3__9)
			
		OPCODE(0x0B, 8, DEC_0_3__B)
		OPCODE(0x1B, 8, DEC_0_3__B)
		OPCODE(0x2B, 8, DEC_0_3__B)
		OPCODE(0x3B, 8, DEC_0_3__B)

		OPCODE(0x0F, 4, RRC_0__F)

		OPCODE(0x10, 4, STOP_1__0)

		OPCODE(0x17, 4, RL_1__7)

		OPCODE(0x18, 8, JR_1__8)

		OPCODE(0x1F, 4, RR_1__F)

		OPCODE(0x20, 8, JR_2_3__0__2_3__8)
		OPCODE(0x28, 8, JR_2_3__0__2_3__8)
		OPCODE(0x30, 8, JR_2_3__0__2_3__8)
		OPCODE(0x38, 8, JR_2_3__0__2_3__8)

		OPCODE(0x22, 8, LDI_2__2)
		OPCODE(0x32, 8, LDD_3__2)
		OPCODE(0x2A, 8, LDI_2__A)
		OPCODE(0x3A, 8, LDD_3__A)

		OPCODE(0x27, 4, DAA_2__7)

		OPCODE(0x2F, 4, CPL_2__F)

		OPCODE(0x37, 4, SCF_3__7)

		OPCODE(0x3F, 4, CCF_3__F)

		OPCODE(0x40, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x41, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x42, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x43, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x44, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x45, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x46, 8, LD_4_7__0_F__NO_7__6)
		OPCODE(0x47, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x48, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x49, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x4A, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x4B, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x4C, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x4D, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x4E, 8, LD_4_7__0_F__NO_7__6)
		OPCODE(0x4F, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x50, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x51, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x52, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x53, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x54, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x55, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x56, 8, LD_4_7__0_F__NO_7__6)
		OPCODE(0x57, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x58, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x59, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x5A, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x5B, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x5C, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x5D, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x5E, 8, LD_4_7__0_F__NO_7__6)
		OPCODE(0x5F, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x60, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x61, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x62, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x63, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x64, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x65, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x66, 8, LD_4_7__0_F__NO_7__6)
		OPCODE(0x67, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x68, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x69, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x6A, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x6B, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x6C, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x6D, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x6E, 8, LD_4_7__0_F__NO_7__6)
		OPCODE(0x6F, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x70, 8, LD_4_7__0_F__NO_7__6)
		OPCODE(0x71, 8, LD_4_7__0_F__NO_7__6)
		OPCODE(0x72, 8, LD_4_7__0_F__NO_7__6)
		OPCODE(0x73, 8, LD_4_7__0_F__NO_7__6)
		OPCODE(0x74, 8, LD_4_7__0_F__NO_7__6)
		OPCODE(0x75, 8, LD_4_7__0_F__NO_7__6)
		OPCODE(0x77, 8, LD_4_7__0_F__NO_7__6)
		OPCODE(0x78, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x79, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x7A, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x7B, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x7C, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x7D, 4, LD_4_7__0_F__NO_7__6)
		OPCODE(0x7E, 8, LD_4_7__0_F__NO_7__6)
		OPCODE(0x7F, 4, LD_4_7__0_F__NO_7__6)
			
		OPCODE(0x76, 4, HALT_7__6)

		OPCODE(0x80, 4, ADD_8__0_7)
		OPCODE(0x81, 4, ADD_8__0_7)
		OPCODE(0x82, 4, ADD_8__0_7)
		OPCODE(0x83, 4, ADD_8__0_7)
		OPCODE(0x84, 4, ADD_8__0_7)
		OPCODE(0x85, 4, ADD_8__0_7)
		OPCODE(0x86, 8, ADD_8__0_7)
		OPCODE(0x87, 4, ADD_8__0_7)

		OPCODE(0x88, 4, ADC_8__8_F)
		OPCODE(0x89, 4, ADC_8__8_F)
		OPCODE(0x8A, 4, ADC_8__8_F)
		OPCODE(0x8B, 4, ADC_8__8_F)
		OPCODE(0x8C, 4, ADC_8__8_F)
		OPCODE(0x8D, 4, ADC_8__8_F)
		OPCODE(0x8E, 8, ADC_8__8_F)
		OPCODE(0x8F, 4, ADC_8__8_F)

		OPCODE(0x90, 4, SUB_9__0_7)
		OPCODE(0x91, 4, SUB_9__0_7)
		OPCODE(0x92, 4, SUB_9__0_7)
		OPCODE(0x93, 4, SUB_9__0_7)
		OPCODE(0x94, 4, SUB_9__0_7)
		OPCODE(0x95, 4, SUB_9__0_7)
		OPCODE(0x96, 8, SUB_9__0_7)
		OPCODE(0x97, 4, SUB_9__0_7)

		OPCODE(0x98, 4, SBC_9__8_F)
		OPCODE(0x99, 4, SBC_9__8_F)
		OPCODE(0x9A, 4, SBC_9__8_F)
		OPCODE(0x9B, 4, SBC_9__8_F)
		OPCODE(0x9C, 4, SBC_9__8_F)
		OPCODE(0x9D, 4, SBC_9__8_F)
		OPCODE(0x9E, 8, SBC_9__8_F)
		OPCODE(0x9F, 4, SBC_9__8_F)

		OPCODE(0xA0, 4, AND_A__0_7)
		OPCODE(0xA1, 4, AND_A__0_7)
		OPCODE(0xA2, 4, AND_A__0_7)
		OPCODE(0xA3, 4, AND_A__0_7)
		OPCODE(0xA4, 4, AND_A__0_7)
		OPCODE(0xA5, 4, AND_A__0_7)
		OPCODE(0xA6, 8, AND_A__0_7)
		OPCODE(0xA7, 4, AND_A__0_7)

		OPCODE(0xA8, 4, XOR_A__8_F)
		OPCODE(0xA9, 4, XOR_A__8_F)
		OPCODE(0xAA, 4, XOR_A__8_F)
		OPCODE(0xAB, 4, XOR_A__8_F)
		OPCODE(0xAC, 4, XOR_A__8_F)
		OPCODE(0xAD, 4, XOR_A__8_F)
		OPCODE(0xAE, 8, XOR_A__8_F)
		OPCODE(0xAF, 4, XOR_A__8_F)

		OPCODE(0xB0, 4, OR_B__0_7)
		OPCODE(0xB1, 4, OR_B__0_7)
		OPCODE(0xB2, 4, OR_B__0_7)
		OPCODE(0xB3, 4, OR_B__0_7)
		OPCODE(0xB4, 4, OR_B__0_7)
		OPCODE(0xB5, 4, OR_B__0_7)
		OPCODE(0xB6, 8, OR_B__0_7)
		OPCODE(0xB7, 4, OR_B__0_7)
		
		OPCODE(0xB8, 4, CP_B__8_F)
		OPCODE(0xB9, 4, CP_B__8_F)
		OPCODE(0xBA, 4, CP_B__8_F)
		OPCODE(0xBB, 4, CP_B__8_F)
		OPCODE(0xBC, 4, CP_B__8_F)
		OPCODE(0xBD, 4, CP_B__8_F)
		OPCODE(0xBE, 8, CP_B__8_F)
		OPCODE(0xBF, 4, CP_B__8_F)

		OPCODE(0xC0, 8, RET_C_D__0__C_D__8)
		OPCODE(0xC8, 8, RET_C_D__0__C_D__8)
		OPCODE(0xD0, 8, RET_C_D__0__C_D__8)
		OPCODE(0xD8, 8, RET_C_D__0__C_D__8)

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

		OPCODE(0xC2, 12, JP_C_D__2__C_D__2)
		OPCODE(0xCA, 12, JP_C_D__2__C_D__2)
		OPCODE(0xD2, 12, JP_C_D__2__C_D__2)
		OPCODE(0xDA, 12, JP_C_D__2__C_D__2)
			
		case 0xC3: // JP nn
			{
				instructionCycles = 12;
				auto target = Fetch16();
				PC = target;
			}
			break;

		OPCODE(0xC4, 12, CALL_C_D__4__C_D__C)
		OPCODE(0xD4, 12, CALL_C_D__4__C_D__C)
		OPCODE(0xCC, 12, CALL_C_D__4__C_D__C)
		OPCODE(0xDC, 12, CALL_C_D__4__C_D__C)

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
			
		OPCODE(0xC6, 8, ADD_C_6)

		OPCODE(0xC7, 32, RST_C_F__7__C_F__F)
		OPCODE(0xD7, 32, RST_C_F__7__C_F__F)
		OPCODE(0xE7, 32, RST_C_F__7__C_F__F)
		OPCODE(0xF7, 32, RST_C_F__7__C_F__F)
		OPCODE(0xCF, 32, RST_C_F__7__C_F__F)
		OPCODE(0xDF, 32, RST_C_F__7__C_F__F)
		OPCODE(0xEF, 32, RST_C_F__7__C_F__F)
		OPCODE(0xFF, 32, RST_C_F__7__C_F__F)

		case 0xC9: // RET
			{
				instructionCycles = 8;
				Ret();
			}
			break;
		case 0xCB: // Extended opcodes
			{
				opcode = Fetch8();
				switch (opcode)
				{
					OPCODE(0x00, 8, RLC_CB_0__0_7)
					OPCODE(0x01, 8, RLC_CB_0__0_7)
					OPCODE(0x02, 8, RLC_CB_0__0_7)
					OPCODE(0x03, 8, RLC_CB_0__0_7)
					OPCODE(0x04, 8, RLC_CB_0__0_7)
					OPCODE(0x05, 8, RLC_CB_0__0_7)
					OPCODE(0x06, 16, RLC_CB_0__0_7)
					OPCODE(0x07, 8, RLC_CB_0__0_7)

					OPCODE(0x08, 8, RRC_CB_0__8_F)
					OPCODE(0x09, 8, RRC_CB_0__8_F)
					OPCODE(0x0A, 8, RRC_CB_0__8_F)
					OPCODE(0x0B, 8, RRC_CB_0__8_F)
					OPCODE(0x0C, 8, RRC_CB_0__8_F)
					OPCODE(0x0D, 8, RRC_CB_0__8_F)
					OPCODE(0x0E, 16, RRC_CB_0__8_F)
					OPCODE(0x0F, 8, RRC_CB_0__8_F)

					OPCODE(0x10, 8, RL_CB_1__0_7)
					OPCODE(0x11, 8, RL_CB_1__0_7)
					OPCODE(0x12, 8, RL_CB_1__0_7)
					OPCODE(0x13, 8, RL_CB_1__0_7)
					OPCODE(0x14, 8, RL_CB_1__0_7)
					OPCODE(0x15, 8, RL_CB_1__0_7)
					OPCODE(0x16, 16, RL_CB_1__0_7)
					OPCODE(0x17, 8, RL_CB_1__0_7)
							 
					OPCODE(0x18, 8, RR_CB_1__8_F)
					OPCODE(0x19, 8, RR_CB_1__8_F)
					OPCODE(0x1A, 8, RR_CB_1__8_F)
					OPCODE(0x1B, 8, RR_CB_1__8_F)
					OPCODE(0x1C, 8, RR_CB_1__8_F)
					OPCODE(0x1D, 8, RR_CB_1__8_F)
					OPCODE(0x1E, 12, RR_CB_1__8_F)
					OPCODE(0x1F, 8, RR_CB_1__8_F)

					OPCODE(0x20, 8, SLA_CB_2__0_7)
					OPCODE(0x21, 8, SLA_CB_2__0_7)
					OPCODE(0x22, 8, SLA_CB_2__0_7)
					OPCODE(0x23, 8, SLA_CB_2__0_7)
					OPCODE(0x24, 8, SLA_CB_2__0_7)
					OPCODE(0x25, 8, SLA_CB_2__0_7)
					OPCODE(0x26, 16, SLA_CB_2__0_7)
					OPCODE(0x27, 8, SLA_CB_2__0_7)

					OPCODE(0x28, 8, SRA_CB_2__8_F)
					OPCODE(0x29, 8, SRA_CB_2__8_F)
					OPCODE(0x2A, 8, SRA_CB_2__8_F)
					OPCODE(0x2B, 8, SRA_CB_2__8_F)
					OPCODE(0x2C, 8, SRA_CB_2__8_F)
					OPCODE(0x2D, 8, SRA_CB_2__8_F)
					OPCODE(0x2E, 16, SRA_CB_2__8_F)
					OPCODE(0x2F, 8, SRA_CB_2__8_F)

					OPCODE(0x30, 8, SWAP_CB_3__0_7)
					OPCODE(0x31, 8, SWAP_CB_3__0_7)
					OPCODE(0x32, 8, SWAP_CB_3__0_7)
					OPCODE(0x33, 8, SWAP_CB_3__0_7)
					OPCODE(0x34, 8, SWAP_CB_3__0_7)
					OPCODE(0x35, 8, SWAP_CB_3__0_7)
					OPCODE(0x36, 16, SWAP_CB_3__0_7)
					OPCODE(0x37, 8, SWAP_CB_3__0_7)

					OPCODE(0x38, 8, SRL_CB_3__8_F)
					OPCODE(0x39, 8, SRL_CB_3__8_F)
					OPCODE(0x3A, 8, SRL_CB_3__8_F)
					OPCODE(0x3B, 8, SRL_CB_3__8_F)
					OPCODE(0x3C, 8, SRL_CB_3__8_F)
					OPCODE(0x3D, 8, SRL_CB_3__8_F)
					OPCODE(0x3E, 16, SRL_CB_3__8_F)
					OPCODE(0x3F, 8, SRL_CB_3__8_F)
				default:
					{
						// Back out and let the unknown opcode handler do its job
						--PC;
						opcode = 0xCB;
						unknownOpcode = true;
					}
					break;
				}
			}
			break;

		case 0xCD: // CALL nn
			{
				instructionCycles = 12;
				auto address = Fetch16();
				Call(address);
			}
			break;

		OPCODE(0xCE, 8, ADC_C__E)

		OPCODE(0xD6, 8, SUB_D__6)

		OPCODE(0xD9, 8, RETI_D__9)
		
		OPCODE(0xDE, 8, SBC_D__E)

		OPCODE(0xE6, 8, AND_E__6)

		OPCODE(0xE9, 4, JP_E__9)

		OPCODE(0xF6, 8, OR_F_6)

		case 0xE0: // LD (0xFF00+n),A
			{
				instructionCycles = 12;
				auto displacement = Fetch8();
				auto address = displacement + 0xFF00;
				Write8(address, A);
			}
			break;

		OPCODE(0xE8, 16, ADD_E__8)

		OPCODE(0xEE, 8, XOR_E__E)

		case 0xEA: // LD (nn),A
			{
				instructionCycles = 16;
				auto address = Fetch16();
				Write8(address, A);
			}
			break;

		OPCODE(0xF0, 12, LDH_F__0)

		OPCODE(0xF3, 4, DI_F__3)
		
		OPCODE(0xF8, 12, LDHL_F__8)

		OPCODE(0xF9, 8, LD_F__9)

		OPCODE(0xFA, 16, LD_F__A);
		
		OPCODE(0xFB, 4, EI_F__B)

		OPCODE(0xFE, 8, CP_F_E);

		OPCODE(0xD3, 4, IllegalOpcode)
		OPCODE(0xDB, 4, IllegalOpcode)
		OPCODE(0xDD, 4, IllegalOpcode)
		OPCODE(0xE3, 4, IllegalOpcode)
		OPCODE(0xE4, 4, IllegalOpcode)
		OPCODE(0xEB, 4, IllegalOpcode)
		OPCODE(0xEC, 4, IllegalOpcode)
		OPCODE(0xED, 4, IllegalOpcode)
		OPCODE(0xF4, 4, IllegalOpcode)
		OPCODE(0xFC, 4, IllegalOpcode)
		OPCODE(0xFD, 4, IllegalOpcode)

#undef OPCODE
		default:
			unknownOpcode = true;
			break;
		}

		// Lower four bits of F are ALWAYS zero
		F &= 0xF0;

		if (unknownOpcode)
		{
			printf("Unknown opcode encountered after %d opcodes: 0x%02lX\n", m_totalOpcodesExecuted, opcode);
			printf("n: 0x%s nn: 0x%s\n", DebugStringPeek8(PC).c_str(), DebugStringPeek16(PC).c_str());
			SDL_assert(false && "Unknown opcode encountered");
		}

		++m_totalOpcodesExecuted;

		return instructionCycles;
	}
private:
	///////////////////////////////////////////////////////////////////////////
	// MemoryBus access
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

	Uint16 Read16(Uint16 address)
	{
		return m_pMemory->Read16(address);
	}

	void Write8(Uint16 address, Uint8 value)
	{
		m_pMemory->Write8(address, value);
	}

	void Write16(Uint16 address, Uint16 value)
	{
		m_pMemory->Write16(address, value);
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

	void SetFlagsForAdd(Uint8 oldValue, Uint8 operand, Uint8 carry, Uint8 flagMask)
	{
		if (flagMask & FlagBitMask::Zero)
		{
			SetZeroFlagFromValue(oldValue + operand + carry);
		}

		if (flagMask & FlagBitMask::Subtract)
		{
			SetFlagValue(FlagBitIndex::Subtract, false);
		}

		if (flagMask & FlagBitMask::HalfCarry)
		{
			SetFlagValue(FlagBitIndex::HalfCarry, (static_cast<Uint16>(GetLow4(oldValue)) + GetLow4(operand) + carry) > 0xF);
		}

		if (flagMask & FlagBitMask::Carry)
		{
			SetFlagValue(FlagBitIndex::Carry, (static_cast<Uint16>(oldValue) + operand + carry) > 0xFF);
		}
	}

	void SetFlagsForSub(Uint8 oldValue, Uint8 operand, Uint8 carry, Uint8 flagMask = FlagBitMask::All)
	{
		if (flagMask & FlagBitMask::Zero)
		{
			SetZeroFlagFromValue(oldValue - (operand + carry));
		}

		if (flagMask & FlagBitMask::Subtract)
		{
			SetFlagValue(FlagBitIndex::Subtract, true);
		}

		if (flagMask & FlagBitMask::HalfCarry)
		{
			SetFlagValue(FlagBitIndex::HalfCarry, static_cast<Uint16>(GetLow4(oldValue)) < GetLow4(operand) + carry);
		}

		if (flagMask & FlagBitMask::Carry)
		{
			SetFlagValue(FlagBitIndex::Carry, static_cast<Uint16>(oldValue) < operand + carry);
		}
	}

	void SetFlagsForAdd16(Uint16 oldValue, Uint16 operand)
	{
		SetFlagValue(FlagBitIndex::Subtract, false);
		SetFlagValue(FlagBitIndex::HalfCarry, (static_cast<Sint32>(GetLow12(oldValue)) + GetLow12(operand)) > 0xFFF);
		SetFlagValue(FlagBitIndex::Carry, (static_cast<Sint32>(oldValue) + operand) > 0xFFFF);
	}

	void SetFlagsForAdd8To16(Uint16 oldValue, Uint8 operand)
	{
		Uint8 u8Displacement = static_cast<Uint8>(operand);
		Uint8 u8sp = static_cast<Uint8>(oldValue);
		SetFlagValue(FlagBitIndex::HalfCarry, (static_cast<Uint16>(GetLow4(u8sp)) + GetLow4(u8Displacement)) > 0xF);
		SetFlagValue(FlagBitIndex::Carry, (static_cast<Uint16>(u8sp) + u8Displacement) > 0xFF);
	}

	void SetZeroFlagFromValue(Uint8 value)
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
	// Interrupts
	///////////////////////////////////////////////////////////////////////////

	bool CallInterruptVectorIfRequired(Uint8 bit, Uint8 vector)
	{
		if (IF & IE & bit)
		{
			IF &= ~bit;
			CallI(vector);
			return true;
		}
		return false;
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

	Uint8& IE;
	Uint8& IF;

	bool m_cpuHalted;
	bool m_cpuStopped;

	Uint32 m_totalOpcodesExecuted;
	bool m_traceEnabled;

	std::shared_ptr<MemoryBus> m_pMemory;
};
