#pragma once

#include "SDL.h"

#include "IMemoryBusDevice.h"

#include <map>
#include <stack>
#include <set>

class MemoryMapper;
class Cpu;
class MemoryBus;

#define ENABLE_ANALYZER 1

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

	void OnPreCall(Uint16 unmappedAddress) ELIDE_IF_ANALYZER_DISABLED
	void OnPreCallInterrupt(Uint16 unmappedAddress) ELIDE_IF_ANALYZER_DISABLED
	void OnPreReturn(Uint16 returnStatementAddress) ELIDE_IF_ANALYZER_DISABLED
	void OnPostReturn() ELIDE_IF_ANALYZER_DISABLED

	void OnVramAccess(MemoryRequestType requestType, Uint16 address, Uint8 value) ELIDE_IF_ANALYZER_DISABLED
	void OnOamAccess(MemoryRequestType requestType, Uint16 address, Uint8 value) ELIDE_IF_ANALYZER_DISABLED
	
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
		//bool usesTimer;
		//bool usesJoypad;
		//bool usesGameLinkPort;
		//bool usesLcd;
		//bool usesSound;
		//bool usesHighRam;
		//Uint32 readCount;
		//Uint32 writeCount;
		//Uint32 executedInstructionCount;
	};
	using AnalyzedFunctionMap = std::map<Analyzer::MappedAddress, Analyzer::AnalyzedFunction>;
	using AnalyzedFunctionStack = std::stack<Analyzer::MappedAddress>;

	std::string DebugStringPeek8(Uint16 address);
	std::string DebugStringPeek16(Uint16 address);
	void DebugNextOpcode();

	MappedAddress GetMappedAddress(Uint16 unmappedAddress);
	MappedAddress GetMappedPC();

	void PushFunction(MappedAddress address);
	void PopFunction();
	AnalyzedFunction& GetTopFunction();
	AnalyzedFunction& GetFunction(MappedAddress address);
	void EnsureGlobalFunctionIsOnStack();

	MemoryMapper* m_pMemoryMapper;
	Cpu* m_pCpu;
	MemoryBus* m_pMemory;
	AnalyzedFunctionMap m_functions;
	AnalyzedFunctionStack m_functionStack;
};