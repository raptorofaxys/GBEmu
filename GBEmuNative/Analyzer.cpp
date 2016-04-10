#include "Analyzer.h"

#if ENABLE_ANALYZER

#include "CpuMetadata.h"

#include "Cpu.h"
#include "GameLinkPort.h"
#include "Joypad.h"
#include "Lcd.h"
#include "Memory.h"
#include "MemoryBus.h"
#include "MemoryMapper.h"
#include "Sound.h"
#include "Timer.h"

#include "TraceLog.h"

#include <map>

// Some ideas about disassembly:
// -perform code flow analysis to track function entry and return points
// -perform data flow analysis to identify function input parameters
// -track number of reads and writes per call within a function to guess about a function being a form of memory copy
// -track use of registers and device access
//   -hmmm if non-CPU devices wind up reading/writing to memory bus other than the DMA controller, we'll need to isolate CPU accesses from other device accesses
// -user-assigned memory cell names
// -user-assigned function names or generated "best guess" names based on function behaviour

Analyzer::Analyzer(MemoryMapper* pMemoryMapper, Cpu* pCpu, MemoryBus* pMemory)
{
	m_pMemoryMapper = pMemoryMapper;
	m_pCpu = pCpu;
	m_pMemory = pMemory;
	EnsureGlobalFunctionIsOnStack();
}

std::string Analyzer::DebugStringPeek8(Uint16 address)
{
	Uint8 value = 0;
	bool success = m_pMemory->SafeRead8(address, value);
	char szBuffer[32];
	_snprintf_s(szBuffer, ARRAY_SIZE(szBuffer), "%02lX", value);
	return success ? szBuffer : "??";
}

std::string Analyzer::DebugStringPeek16(Uint16 address)
{
	return DebugStringPeek8(address + 1).append(DebugStringPeek8(address));
}

void Analyzer::DebugNextOpcode()
{
	if (TraceLog::IsEnabled())
	{
		DisableMemoryTrackingForScope dmtfs(*this);

		const auto& meta = CpuMetadata::GetOpcodeMetadata(m_pMemory->Read8(m_pCpu->GetPC()), m_pMemory->Read8(m_pCpu->GetPC() + 1));
		std::string mnemonic = meta.fullMnemonic;

		// Do some "smart" replacements to improve legibility for many opcodes
		if (meta.baseMnemonic == "LDH")
		{
			mnemonic = Replace(mnemonic, "(n)", Format("(FF%sh)", DebugStringPeek8(m_pCpu->GetPC() + 1).c_str()));
		}
		else if (meta.baseMnemonic == "JR")
		{
			mnemonic = Replace(mnemonic, "n", Format("%sh (%04lX)", DebugStringPeek8(m_pCpu->GetPC() + 1).c_str(), m_pCpu->GetPC() + 2 + static_cast<Sint8>(m_pMemory->Read8(m_pCpu->GetPC() + 1))));
		}
		else //if (meta.size == 3)
		{
			mnemonic = Replace(mnemonic, "(nn)", Format("(%sh)", DebugStringPeek16(m_pCpu->GetPC() + 1).c_str()));
			mnemonic = Replace(mnemonic, "nn", Format("%sh", DebugStringPeek16(m_pCpu->GetPC() + 1).c_str()));
			mnemonic = Replace(mnemonic, "(n)", Format("(%sh)", DebugStringPeek8(m_pCpu->GetPC() + 1).c_str()));
			mnemonic = Replace(mnemonic, "n", Format("%sh", DebugStringPeek8(m_pCpu->GetPC() + 1).c_str()));
		}
		//else if (meta.size == 2)
		//{
		//	mnemonic = Replace(mnemonic, "(n)", Format("(n==%sh)", DebugStringPeek8(m_pCpu->GetPC() + 1).c_str()));
		//	mnemonic = Replace(mnemonic, "n", Format("n==%sh", DebugStringPeek8(m_pCpu->GetPC() + 1).c_str()));
		//}

		//TraceLog::Log(Format("-----\n0x%04lX  %02lX   %s  \n", m_pCpu->GetPC(), opcode, pMnemonic));
		//TraceLog::Log(Format("A: 0x%02lX F: %s%s%s%s B: 0x%02lX C: 0x%02lX D: 0x%02lX E: 0x%02lX H: 0x%02lX L: 0x%02lX\n",
		//	A,
		//	GetFlagValue(FlagBitIndex::Zero) ? "Z" : "z",
		//	GetFlagValue(FlagBitIndex::Subtract) ? "S" : "s",
		//	GetFlagValue(FlagBitIndex::HalfCarry) ? "H" : "h",
		//	GetFlagValue(FlagBitIndex::Carry) ? "C" : "c", 
		//	B, C, D, E, H, L));
		//TraceLog::Log(Format("AF: 0x%04lX BC: 0x%04lX DE: 0x%04lX HL: 0x%04lX SP: 0x%04lX IME: %d\n", AF, BC, DE, HL, SP, IME ? 1 : 0));
		//TraceLog::Log(Format("n: 0x%s nn: 0x%s\n", DebugStringPeek8(m_pCpu->GetPC() + 1).c_str(), DebugStringPeek16(m_pCpu->GetPC() + 1).c_str()));
		////printf("(BC): 0x%s (DE): 0x%s (HL): 0x%s (nn): 0x%s\n", DebugStringPeek8(BC), DebugStringPeek8(DE), DebugStringPeek8(HL), DebugStringPeek16(Peek16(m_pCpu->GetPC() + 1)));
		//TraceLog::Log(Format("(BC): 0x%s (DE): 0x%s (HL): 0x%s\n", DebugStringPeek8(BC).c_str(), DebugStringPeek8(DE).c_str(), DebugStringPeek8(HL).c_str()));
		// Format partly inspired from VisualBoyAdvance and then bastardized...
		TraceLog::Log(Format("CPU %08d: [%04x] %-16s AF=%02X%02X BC=%02X%02X DE=%02X%02X HL=%02X%02X SP=%04X %c%c%c%c LY=%d %c%c\n",
			m_pCpu->GetTotalExecutedOpcodes(),
			m_pCpu->GetPC(),
			mnemonic.c_str(),
			m_pCpu->GetA(),
			m_pCpu->GetF(),
			m_pCpu->GetB(),
			m_pCpu->GetC(),
			m_pCpu->GetD(),
			m_pCpu->GetE(),
			m_pCpu->GetH(),
			m_pCpu->GetL(),
			m_pCpu->GetSP(),
			m_pCpu->GetFlagValue(FlagBitIndex::Zero) ? 'Z' : 'z',
			m_pCpu->GetFlagValue(FlagBitIndex::Subtract) ? 'S' : 's',
			m_pCpu->GetFlagValue(FlagBitIndex::HalfCarry) ? 'H' : 'h',
			m_pCpu->GetFlagValue(FlagBitIndex::Carry) ? 'C' : 'c',
			m_pMemory->Read8(static_cast<Uint16>(Lcd::Registers::LY)),
			((m_pCpu->GetIF() & Bit1) != 0) ? 'V' : 'v',
			m_pCpu->GetIME() ? 'I' : 'i'));
	}
}

void Analyzer::SetTracingEnabled(bool enabled)
{
	TraceLog::SetEnabled(enabled);
}

void Analyzer::FlushTrace()
{
	TraceLog::Flush();
}

void Analyzer::OnStart(const char * pRomName)
{
	TraceLog::Reset();
	TraceLog::Log(Format("\n\nNew run on %s\n\n", pRomName));
	EnsureGlobalFunctionIsOnStack();
	m_trackMemoryAccesses = true;
}

void Analyzer::OnPreExecuteOpcode()
{
	DebugNextOpcode();
	++GetTopFunction().executedInstructionCount;
}

void Analyzer::OnPreCall(Uint16 unmappedAddress)
{
	auto ma = GetMappedAddress(unmappedAddress);
	GetFunction(ma);
	PushFunction(ma);
}

void Analyzer::OnPreCallInterrupt(Uint16 unmappedAddress)
{
	auto ma = GetMappedAddress(unmappedAddress);
	PushFunction(ma);
	GetTopFunction().isInterruptServiceRoutine = true;
}

void Analyzer::OnPreReturn(Uint16 returnStatementAddress)
{
	auto ma = GetMappedPC();
	auto& func = GetTopFunction();
	func.exitPoints.insert(ma);
	PopFunction();
}

void Analyzer::OnPostReturn()
{
	//@TODO: validate that the return address is the expected one in case the software is doing funny things to the stack? would require storing the expected return address on the stack
	//auto ma = GetMappedPC();
}

#define TRACK_DEVICE_USAGE(device, reg) if (address == static_cast<Uint16>(device::Registers::reg)) { func.uses##device = true; }
#define TRACK_DEVICES() \
	TRACK_DEVICE_USAGE(Timer, DIV) \
	TRACK_DEVICE_USAGE(Timer, TIMA) \
	TRACK_DEVICE_USAGE(Timer, TMA) \
	TRACK_DEVICE_USAGE(Timer, TAC) \
	TRACK_DEVICE_USAGE(Joypad, P1_JOYP) \
	TRACK_DEVICE_USAGE(GameLinkPort, SB) \
	TRACK_DEVICE_USAGE(GameLinkPort, SC) \
	TRACK_DEVICE_USAGE(Lcd, LCDC) \
	TRACK_DEVICE_USAGE(Lcd, STAT) \
	TRACK_DEVICE_USAGE(Lcd, SCY) \
	TRACK_DEVICE_USAGE(Lcd, SCX) \
	TRACK_DEVICE_USAGE(Lcd, LY) \
	TRACK_DEVICE_USAGE(Lcd, LYC) \
	TRACK_DEVICE_USAGE(Lcd, DMA) \
	TRACK_DEVICE_USAGE(Lcd, BGP) \
	TRACK_DEVICE_USAGE(Lcd, OBP0) \
	TRACK_DEVICE_USAGE(Lcd, OBP1) \
	TRACK_DEVICE_USAGE(Lcd, WY) \
	TRACK_DEVICE_USAGE(Lcd, WX) \
	TRACK_DEVICE_USAGE(Sound, NR10) \
	TRACK_DEVICE_USAGE(Sound, NR11) \
	TRACK_DEVICE_USAGE(Sound, NR12) \
	TRACK_DEVICE_USAGE(Sound, NR13) \
	TRACK_DEVICE_USAGE(Sound, NR14) \
	TRACK_DEVICE_USAGE(Sound, NR21) \
	TRACK_DEVICE_USAGE(Sound, NR22) \
	TRACK_DEVICE_USAGE(Sound, NR23) \
	TRACK_DEVICE_USAGE(Sound, NR24) \
	TRACK_DEVICE_USAGE(Sound, NR30) \
	TRACK_DEVICE_USAGE(Sound, NR31) \
	TRACK_DEVICE_USAGE(Sound, NR32) \
	TRACK_DEVICE_USAGE(Sound, NR33) \
	TRACK_DEVICE_USAGE(Sound, NR34) \
	TRACK_DEVICE_USAGE(Sound, NR41) \
	TRACK_DEVICE_USAGE(Sound, NR42) \
	TRACK_DEVICE_USAGE(Sound, NR43) \
	TRACK_DEVICE_USAGE(Sound, NR44) \
	TRACK_DEVICE_USAGE(Sound, NR50) \
	TRACK_DEVICE_USAGE(Sound, NR51) \
	TRACK_DEVICE_USAGE(Sound, NR52)

void Analyzer::OnPostRead8(Uint16 address, Uint8 value)
{
	if (!m_trackMemoryAccesses)
	{
		return;
	}

	auto& func = GetTopFunction();
	++func.readCount;
	TRACK_DEVICES();
	if (IsAddressInRange(address, Memory::kHramMemoryBase, Memory::kHramMemorySize))
	{
		func.usesHighRam = true;
	}
}

void Analyzer::OnPostWrite8(Uint16 address, Uint8 value)
{
	if (!m_trackMemoryAccesses)
	{
		return;
	}

	auto& func = GetTopFunction();
	++func.writeCount;
	TRACK_DEVICES();
	if (IsAddressInRange(address, Memory::kHramMemoryBase, Memory::kHramMemorySize))
	{
		func.usesHighRam = true;
	}
}
#undef TRACK_DEVICE_USAGE

void Analyzer::OnPostVramAccess(MemoryRequestType requestType, Uint16 address, Uint8 value)
{
	GetTopFunction().usesLcd = true;
	GetTopFunction().usesVram = true;
}

void Analyzer::OnPostOamAccess(MemoryRequestType requestType, Uint16 address, Uint8 value)
{
	GetTopFunction().usesLcd = true;
	GetTopFunction().usesOam = true;
}

void Analyzer::OnPostRomBankSwitch(Uint8 bankIndex)
{
	GetTopFunction().usesMapper = true;
}

void Analyzer::OnPostBankingModeSwitch()
{
	GetTopFunction().usesMapper = true;
}

void Analyzer::OnUnknownOpcode(Uint16 unmappedAddress)
{
	//printf("Unknown opcode encountered after %d opcodes: 0x%02lX\n", m_totalOpcodesExecuted, opcode);
	//printf("n: 0x%s nn: 0x%s\n", DebugStringPeek8(m_pCpu->GetPC()).c_str(), DebugStringPeek16(m_pCpu->GetPC()).c_str());
}

Analyzer::MappedAddress Analyzer::GetMappedAddress(Uint16 unmappedAddress)
{
	SDL_assert(m_pMemoryMapper != nullptr);
	return MappedAddress(m_pMemoryMapper->GetActiveBank(), unmappedAddress);
}

Analyzer::MappedAddress Analyzer::GetMappedPC()
{
	return GetMappedAddress(m_pCpu->GetPC());
}

void Analyzer::PushFunction(Analyzer::MappedAddress address)
{
	m_functionStack.push(address);
	m_pTopFunction = &GetTopFunctionFromStack();
}

void Analyzer::PopFunction()
{
	m_functionStack.pop();

	// Failsafe
	if (m_functionStack.size() == 0)
	{
		EnsureGlobalFunctionIsOnStack();
	}
	m_pTopFunction = &GetTopFunctionFromStack();
}

Analyzer::AnalyzedFunction& Analyzer::GetTopFunction()
{
	return *m_pTopFunction;
}

Analyzer::AnalyzedFunction& Analyzer::GetTopFunctionFromStack()
{
	return GetFunction(m_functionStack.top());
}

Analyzer::AnalyzedFunction& Analyzer::GetFunction(Analyzer::MappedAddress address)
{
	auto func = AnalyzedFunction();
	func.entryPoint = address;
	return m_functions.insert(std::make_pair(address, func)).first->second;
}

void Analyzer::EnsureGlobalFunctionIsOnStack()
{
	auto& ma = MappedAddress(0, 0x100);
	GetFunction(ma);
	if (m_functionStack.size() == 0)
	{
		PushFunction(ma);
	}
}

#endif // #if ENABLE_ANALYZER
