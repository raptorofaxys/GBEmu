#pragma once

#include "SDL.h"

#include "IMemoryBusDevice.h"

#include <map>
#include <stack>
#include <set>
#include <unordered_map>

class MemoryMapper;
class Cpu;
class MemoryBus;

#define ENABLE_ANALYZER 0

#if ENABLE_ANALYZER
#define ELIDE_IF_ANALYZER_DISABLED ;
#else
#define ELIDE_IF_ANALYZER_DISABLED {}
#endif

class Analyzer
{
public:
	Analyzer(MemoryMapper* pMemoryMapper, Cpu* pCpu, MemoryBus* pMemory) ELIDE_IF_ANALYZER_DISABLED

	void SetTracingEnabled(bool enabled) ELIDE_IF_ANALYZER_DISABLED
	void FlushTrace() ELIDE_IF_ANALYZER_DISABLED

	void OnStart(const char* pRomName) ELIDE_IF_ANALYZER_DISABLED

    void OnPreExecuteOpcode() ELIDE_IF_ANALYZER_DISABLED
    void OnOpcodeExecutionSkipped() ELIDE_IF_ANALYZER_DISABLED

    void OnHalt() ELIDE_IF_ANALYZER_DISABLED
    void OnHaltResumed(Uint8 IF) ELIDE_IF_ANALYZER_DISABLED

	void OnPreCall(Uint16 unmappedAddress) ELIDE_IF_ANALYZER_DISABLED
	void OnPreCallInterrupt(Uint16 unmappedAddress) ELIDE_IF_ANALYZER_DISABLED
	void OnPreReturn(Uint16 returnStatementAddress) ELIDE_IF_ANALYZER_DISABLED
	void OnPostReturn() ELIDE_IF_ANALYZER_DISABLED

	void OnPostRead8(Uint16 address, Uint8 value) ELIDE_IF_ANALYZER_DISABLED
	void OnPostWrite8(Uint16 address, Uint8 value) ELIDE_IF_ANALYZER_DISABLED

	void OnPostVramAccess(MemoryRequestType requestType, Uint16 address, Uint8 value) ELIDE_IF_ANALYZER_DISABLED
	void OnPostOamAccess(MemoryRequestType requestType, Uint16 address, Uint8 value) ELIDE_IF_ANALYZER_DISABLED

	void OnPostRomBankSwitch(Uint8 bankIndex) ELIDE_IF_ANALYZER_DISABLED
	void OnPostBankingModeSwitch() ELIDE_IF_ANALYZER_DISABLED
	
	void OnUnknownOpcode(Uint16 unmappedAddress) ELIDE_IF_ANALYZER_DISABLED

private:
	struct MappedAddress
	{
		Uint8 bank = 0;
		Uint16 address = 0;
		MappedAddress() {}
		MappedAddress(Uint8 bank_, Uint16 address_) { bank = bank_; address = address_; }
		bool operator<(const MappedAddress& other) const { return (bank < other.bank) ? true : ((bank > other.bank) ? false : (address < other.address)); }
	};

	struct AnalyzedFunction
	{
		Analyzer::MappedAddress entryPoint;
		std::set<Analyzer::MappedAddress> exitPoints;
		bool isInterruptServiceRoutine = false;
		bool usesTimer = false;
		bool usesJoypad = false;
		bool usesGameLinkPort = false;
		bool usesLcd = false;
		bool usesVram = false;
		bool usesOam = false;
		bool usesSound = false;
		bool usesMapper = false;
		bool usesHighRam;
		Uint32 readCount = 0;
		Uint32 writeCount = 0;
		Uint32 executedInstructionCount = 0;
	};
	using AnalyzedFunctionMap = std::map<Analyzer::MappedAddress, Analyzer::AnalyzedFunction>;
	using AnalyzedFunctionStack = std::stack<Analyzer::MappedAddress>;

	//using OperandFormatFunc = void(int);
	//std::unordered_map<std::string, OperandFormatFunc> m_operandFormatters;
	void Annotate_n(std::string& operandString, std::string& comment);
	void Annotate_nn(std::string& operandString, std::string& comment);
	void Annotate_ind_nn(std::string& operandString, std::string& comment);

	void ParseOperand8(Uint16 address, Uint8& op8, std::string& debugString8, bool& success);
	void ParseOperand16(Uint16 address, Uint16& op16, std::string& debugString16, bool& success);
	std::string GetAddressAnnotation(Uint16 address);
	void DebugNextOpcode();

	MappedAddress GetMappedAddress(Uint16 unmappedAddress);
	MappedAddress GetMappedPC();

	void PushFunction(MappedAddress address);
	void PopFunction();
	AnalyzedFunction& GetTopFunction(); // this is an optimization
	AnalyzedFunction& GetTopFunctionFromStack(); //equivalent to GetTopFunction but slow
	AnalyzedFunction& GetFunction(MappedAddress address);
	void EnsureGlobalFunctionIsOnStack();

	std::unordered_map<std::string, void(Analyzer::*)(std::string&, std::string&)> m_operandFormatters;

	MemoryMapper* m_pMemoryMapper;
	Cpu* m_pCpu;
	MemoryBus* m_pMemory;
	AnalyzedFunctionMap m_functions;
	AnalyzedFunctionStack m_functionStack;
	AnalyzedFunction* m_pTopFunction;

	struct DisableMemoryTrackingForScope
	{
		DisableMemoryTrackingForScope(Analyzer& analyzer)
			: m_analyzer(analyzer)
		{
			m_restoreValue = m_analyzer.m_trackMemoryAccesses;
		}
		
		~DisableMemoryTrackingForScope()
		{
			m_analyzer.m_trackMemoryAccesses = m_restoreValue;
		}

		Analyzer& m_analyzer;
		bool m_restoreValue;
	};

	bool m_trackMemoryAccesses = false;
};