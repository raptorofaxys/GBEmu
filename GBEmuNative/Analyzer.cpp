#include "Analyzer.h"

#if ENABLE_ANALYZER

#include "CpuMetadata.h"

#include "Cpu.h"
#include "Lcd.h"
#include "MemoryBus.h"
#include "MemoryMapper.h"

#include "TraceLog.h"

#include <map>

// Some ideas about disassembly:
// -perform code flow analysis to track function entry and return points
// -perform data flow analysis to identify function input parameters
// -track number of reads and writes per call within a function to guess about a function being a form of memory copy
// -track use of registers and device access
// -user-assigned memory cell names
// -user-assigned function names or generated "best guess" names based on function behaviour

Analyzer::Analyzer(MemoryMapper* pMemoryMapper, Cpu* pCpu, MemoryBus* pMemory)
{
	m_pMemoryMapper = pMemoryMapper;
	m_pCpu = pCpu;
	m_pMemory = pMemory;
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
		const auto& meta = CpuMetadata::GetOpcodeMetadata(m_pMemory->Read8(m_pCpu->GetPC()), m_pMemory->Read8(m_pCpu->GetPC() + 1));
		std::string mnemonic = meta.baseMnemonic;

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
}

void Analyzer::OnPreExecuteOpcode()
{
	DebugNextOpcode();
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

void Analyzer::OnVramAccess(MemoryRequestType requestType, Uint16 address, Uint8 value)
{
}

void Analyzer::OnOamAccess(MemoryRequestType requestType, Uint16 address, Uint8 value)
{
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
}

void Analyzer::PopFunction()
{
	m_functionStack.pop();

	// Failsafe
	if (m_functionStack.size() == 0)
	{
		EnsureGlobalFunctionIsOnStack();
	}
}

Analyzer::AnalyzedFunction& Analyzer::GetTopFunction()
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
