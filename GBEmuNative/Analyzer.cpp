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
#include <unordered_map>

// Some ideas about disassembly:
// -perform code flow analysis to track function entry and return points
// -perform data flow analysis to identify function input parameters, function output parameters (analyze use of registers after function return; beware of interrupt calls)
// -track number of reads and writes per call within a function to guess about a function being a form of memory copy
// -track use of registers and device access
//   -hmmm if non-CPU devices wind up reading/writing to memory bus other than the DMA controller, we'll need to isolate CPU accesses from other device accesses
// -user-assigned memory cell names
// -user-assigned function names or generated "best guess" names based on function behaviour

#define ENUMERATE_DEVICES() \
	ENUMERATE_DEVICE(Timer, DIV) \
	ENUMERATE_DEVICE(Timer, TIMA) \
	ENUMERATE_DEVICE(Timer, TMA) \
	ENUMERATE_DEVICE(Timer, TAC) \
	ENUMERATE_DEVICE(Joypad, P1_JOYP) \
	ENUMERATE_DEVICE(GameLinkPort, SB) \
	ENUMERATE_DEVICE(GameLinkPort, SC) \
	ENUMERATE_DEVICE(Lcd, LCDC) \
	ENUMERATE_DEVICE(Lcd, STAT) \
	ENUMERATE_DEVICE(Lcd, SCY) \
	ENUMERATE_DEVICE(Lcd, SCX) \
	ENUMERATE_DEVICE(Lcd, LY) \
	ENUMERATE_DEVICE(Lcd, LYC) \
	ENUMERATE_DEVICE(Lcd, DMA) \
	ENUMERATE_DEVICE(Lcd, BGP) \
	ENUMERATE_DEVICE(Lcd, OBP0) \
	ENUMERATE_DEVICE(Lcd, OBP1) \
	ENUMERATE_DEVICE(Lcd, WY) \
	ENUMERATE_DEVICE(Lcd, WX) \
	ENUMERATE_DEVICE(Sound, NR10) \
	ENUMERATE_DEVICE(Sound, NR11) \
	ENUMERATE_DEVICE(Sound, NR12) \
	ENUMERATE_DEVICE(Sound, NR13) \
	ENUMERATE_DEVICE(Sound, NR14) \
	ENUMERATE_DEVICE(Sound, NR21) \
	ENUMERATE_DEVICE(Sound, NR22) \
	ENUMERATE_DEVICE(Sound, NR23) \
	ENUMERATE_DEVICE(Sound, NR24) \
	ENUMERATE_DEVICE(Sound, NR30) \
	ENUMERATE_DEVICE(Sound, NR31) \
	ENUMERATE_DEVICE(Sound, NR32) \
	ENUMERATE_DEVICE(Sound, NR33) \
	ENUMERATE_DEVICE(Sound, NR34) \
	ENUMERATE_DEVICE(Sound, NR41) \
	ENUMERATE_DEVICE(Sound, NR42) \
	ENUMERATE_DEVICE(Sound, NR43) \
	ENUMERATE_DEVICE(Sound, NR44) \
	ENUMERATE_DEVICE(Sound, NR50) \
	ENUMERATE_DEVICE(Sound, NR51) \
	ENUMERATE_DEVICE(Sound, NR52)

//namespace
//{
//	auto OperandFormatFuncDummy = [](const std::string& operand, const Analyzer& analyzer, const Cpu& cpu, std::string& annotation, std::string& comment) {};
//	using OperandFormatFunc = decltype(OperandFormatFuncDummy);
//	static std::unordered_map<std::string, OperandFormatFunc> s_operandFormatters;
//	//static std::unordered_map<std::string, decltype([](const std::string& operand, const Cpu& cpu) {})> s_operandFormatters;
//
//	struct Initializer
//	{
//		Initializer()
//		{
//			s_operandFormatters["n"] = [](const std::string& operand, const Analyzer& analyzer, const Cpu& cpu, std::string& annotation, std::string& comment) { analyzer.m_pCpu; };
//			// Figure this out. Either:
//			// -use member functions on the Analyzer
//			// -use non-member lambdas and add accessors on the Analyzer for its state
//
//			// If the operand is indirect and the address is invalid, this needs to be apparent
//			// If the operand is an immediate address, we can annotate it directly and move the immediate to the comment
//			// IF the operand is a register indirect address, leave it as is but annotate in the comment if possible
//		}
//	};
//	Initializer g_initializer;
//}

Analyzer::Analyzer(MemoryMapper* pMemoryMapper, Cpu* pCpu, MemoryBus* pMemory)
{
	m_operandFormatters["n"] = &Analyzer::Annotate_n;
	m_operandFormatters["nn"] = &Analyzer::Annotate_nn;
	m_operandFormatters["(nn)"] = &Analyzer::Annotate_ind_nn;

	m_pMemoryMapper = pMemoryMapper;
	m_pCpu = pCpu;
	m_pMemory = pMemory;
	EnsureGlobalFunctionIsOnStack();
}

std::string Format8(Uint8 value)
{
	return Format("%02lXh", value);
}

std::string Format16(Uint16 value)
{
	return Format("%04lXh", value);
}

std::string Format8(Uint8 value, bool success)
{
	return success ? Format8(value) : "??h";
}

std::string Format16(Uint16 value, bool success)
{
	return success ? Format16(value) : "????h";
}

void Analyzer::Annotate_n(std::string& operandString, std::string& comment)
{
	auto op8 = Uint8(0);
	auto op8str = std::string();
	bool op8success = false;
	ParseOperand8(m_pCpu->GetPC() + 1, op8, op8str, op8success);
	operandString = op8str;
}

void Analyzer::Annotate_nn(std::string& operandString, std::string& comment)
{
	auto op16 = Uint16(0);
	auto op16str = std::string();
	bool op16success = false;
	ParseOperand16(m_pCpu->GetPC() + 1, op16, op16str, op16success);
	operandString = op16str;
}

void Analyzer::Annotate_ind_nn(std::string& operandString, std::string& comment)
{
	auto op16 = Uint16(0);
	auto op16str = std::string();
	bool op16success = false;
	ParseOperand16(m_pCpu->GetPC() + 1, op16, op16str, op16success);
	operandString = Format("(%s)", op16str.c_str());
	if (op16success)
	{
		auto annotation = GetAddressAnnotation(op16);
		if (!annotation.empty())
		{
			comment = operandString;
			operandString = Format("(%s)", annotation);
		}
	}
}

void Analyzer::ParseOperand8(Uint16 address, Uint8& op8, std::string& debugString8, bool& success)
{
	success = m_pMemory->SafeRead8(address, op8);
	debugString8 = Format8(op8, success);
}

void Analyzer::ParseOperand16(Uint16 address, Uint16& op16, std::string& debugString16, bool& success)
{
	success = m_pMemory->SafeRead16(address, op16);
	debugString16 = Format16(op16, success);
}

std::string Analyzer::GetAddressAnnotation(Uint16 address)
{
	switch (address)
	{
#define ENUMERATE_DEVICE(device, reg) case device::Registers::reg: return std::string(#reg); break;
		ENUMERATE_DEVICES()
#undef ENUMERATE_DEVICE
	}
	return "";
}

void Analyzer::DebugNextOpcode()
{
	if (TraceLog::IsEnabled())
	{
		DisableMemoryTrackingForScope dmtfs(*this);

		//@OPTIMIZE: lots of duplicate accesses to memory around the PC for ease of formatting and parsing. This could be streamlined.
		auto pc = m_pCpu->GetPC();

		const auto& meta = CpuMetadata::GetOpcodeMetadata(m_pMemory->SafeRead8(pc), m_pMemory->SafeRead8(pc + 1));
		std::string mnemonic = meta.fullMnemonic;

		//auto op8 = Uint8(0);
		//auto op8str = std::string();
		//bool op8success = false;
		//ParseOperand8(pc + 1, op8, op8str, op8success);

		//auto op16 = Uint16(0);
		//auto op16str = std::string();
		//bool op16success = false;
		//ParseOperand16(pc + 1, op16, op16str, op16success);

		//auto inputComment = std::string();
		//auto outputComment = std::string();

		//auto OperandFormatFuncDummy =
		//	[this, &meta, op8, &op8str, &op8success, op16, &op16str, &op16success]
		//	(std::string& annotation, std::string& comment) {};
		//using OperandFormatFunc = decltype(OperandFormatFuncDummy);
		//static std::unordered_map<std::string, OperandFormatFunc> s_operandFormatters;
		//struct OperandFormatFuncsInitializer
		//{
		//	Initializer()
		//	{
		//		s_operandFormatters["n"] = [](const std::string& operand, const Analyzer& analyzer, const Cpu& cpu, std::string& annotation, std::string& comment) { analyzer.m_pCpu; };
		//		// If the operand is indirect and the address is invalid, this needs to be apparent
		//		// If the operand is an immediate address, we can annotate it directly and move the immediate to the comment
		//		// IF the operand is a register indirect address, leave it as is but annotate in the comment if possible
		//	}
		//};
		//Initializer g_initializer;

		auto formatOperand = [this](std::string& operand, std::string& comment)
		{
			auto formatIt = m_operandFormatters.find(operand);
			if (formatIt != m_operandFormatters.end())
			{
				((this)->*(formatIt->second))(operand, comment);
			}
		};

		std::string directOutputOperand = meta.directOutput;
		std::string directOutputComment;
		if (meta.HasDirectOutput())
		{
			formatOperand(directOutputOperand, directOutputComment);
		}

		std::string directInputOperand = meta.directInput;
		std::string directInputComment;
		if (meta.HasDirectInput())
		{
			formatOperand(directInputOperand, directInputComment);
		}

		std::string finalComment = directOutputComment;
		if (!directInputComment.empty())
		{
			if (!finalComment.empty())
			{
				finalComment += " / ";
			}
			finalComment += directInputComment;
		}

		//// Do some "smart" replacements to improve legibility for many opcodes
		//if (meta.baseMnemonic == "LDH")
		//{
		//	auto address = Make16(0xFF, op8);
		//	auto annotation = op8success ? GetAddressAnnotation(address) : std::string("");
		//	auto addressStr = Format16(address, op8success);
		//	auto substitutionString = Format("(%sh)", addressStr.c_str());
		//	if (annotation.length() > 0)
		//	{
		//		// If we have an annotation, use that instead
		//		comment = substitutionString;
		//		substitutionString = Format("(%s)", annotation.c_str());
		//	}
		//	mnemonic = ReplaceFirst(mnemonic, "(n)", substitutionString);
		//}
		//else if (meta.baseMnemonic == "JR")
		//{
		//	mnemonic = ReplaceFirst(mnemonic, "n", Format("%sh", op8str.c_str()));
		//	comment = Format("(%sh)", Format16(pc + 2 + static_cast<Sint8>(op8), op8success).c_str());
		//}

		//if ((meta.baseMnemonic == "JR") || (meta.baseMnemonic == "JP") || (meta.baseMnemonic == "CALL"))
		//{
		//	// Add information about flag state
		//	//mnemonic = ReplaceFirst(mnemonic, " Z", m_pCpu->GetFlagValue(FlagBitIndex::Zero) ? " Z" : " z");
		//	//mnemonic = ReplaceFirst(mnemonic, " NZ", !m_pCpu->GetFlagValue(FlagBitIndex::Zero) ? " NZ" : " nz");
		//	//mnemonic = ReplaceFirst(mnemonic, " C", m_pCpu->GetFlagValue(FlagBitIndex::Carry) ? " C" : " C");
		//	//mnemonic = ReplaceFirst(mnemonic, " NC", !m_pCpu->GetFlagValue(FlagBitIndex::Carry) ? " NC" : " nc");
		//}

		//@TOOD: possibly replace the format strings for the mnemonic table with clearer substitution tokens
		//mnemonic = Replace(mnemonic, "(nn)", Format("(%sh)", op16str)); //@TODO: useless until we print target value, this is covered by the next line
		//mnemonic = ReplaceFirst(mnemonic, "nn", Format("%sh", op16str.c_str()));
		////mnemonic = Replace(mnemonic, "(n)", Format("(%sh)", DebugStringPeek8(m_pCpu->GetPC() + 1).c_str())); // all (n) are LDH
		//mnemonic = ReplaceFirst(mnemonic, "n", Format("%sh", op8str.c_str()));

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
		TraceLog::Log(Format("CPU %08d: [%04X] %-16s %-10s AF=%02X%02X BC=%02X%02X DE=%02X%02X HL=%02X%02X SP=%04X %c%c%c%c LY=%d %c%c\n",
			m_pCpu->GetTotalExecutedOpcodes(),
			m_pCpu->GetPC(),
			mnemonic.c_str(),
			finalComment.length() > 0 ? Format("; %s", finalComment.c_str()).c_str() : "",
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
			m_pMemory->SafeRead8(static_cast<Uint16>(Lcd::Registers::LY)),
			((m_pCpu->GetIF() & Bit1) != 0) ? 'V' : 'v',
			m_pCpu->GetIME() ? 'I' : 'i'));
		
		//@TODO: add traced opcode to function?
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

void Analyzer::OnHalt()
{
    if (TraceLog::IsEnabled())
    {
        TraceLog::Log("(HALT executed, CPU halted)\n");
    }
}

void Analyzer::OnHaltResumed(Uint8 IF)
{
    if (TraceLog::IsEnabled())
    {
        TraceLog::Log(Format("(resuming after HALT - IF %d)\n", IF));
    }
}

void Analyzer::OnPreExecuteOpcode()
{
	DebugNextOpcode();
	++GetTopFunction().executedInstructionCount;
}

void Analyzer::OnOpcodeExecutionSkipped()
{
    if (TraceLog::IsEnabled())
    {
        //TraceLog::Log(Format("(CPU halted, will not execute opcode at 0x%02lX)\n", m_pCpu->GetPC()));
    }
}

void Analyzer::OnPreCall(Uint16 unmappedAddress)
{
	auto ma = GetMappedAddress(unmappedAddress);
	//GetFunction(ma);
	PushFunction(ma);
}

void Analyzer::OnPreCallInterrupt(Uint16 unmappedAddress)
{
	auto ma = GetMappedAddress(unmappedAddress);
	//GetFunction(ma);
	PushFunction(ma);
	GetTopFunction().isInterruptServiceRoutine = true;

    if (TraceLog::IsEnabled())
    {
        TraceLog::Log(Format("(interrupt 0x%04lX pending - CPU unhalted)\n", unmappedAddress));
    }
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

#define ENUMERATE_DEVICE(device, reg) case device::Registers::reg: func.uses##device = true; break;

void Analyzer::OnPostRead8(Uint16 address, Uint8 value)
{
	if (!m_trackMemoryAccesses)
	{
		return;
	}

	auto& func = GetTopFunction();
	++func.readCount;
	switch (address)
	{
		ENUMERATE_DEVICES()
	}
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
	switch (address)
	{
		ENUMERATE_DEVICES()
	}
	if (IsAddressInRange(address, Memory::kHramMemoryBase, Memory::kHramMemorySize))
	{
		func.usesHighRam = true;
	}
}
#undef ENUMERATE_DEVICE

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

	// Failsafe - some games do weird stack manipulation...
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
