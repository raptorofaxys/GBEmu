#pragma once

#include "SDL.h"

#include "IMemoryBusDevice.h"

class MemoryMapper;
class Cpu;
class MemoryBus;

class Analyzer
{
public:
	Analyzer(MemoryMapper* pMemoryMapper, Cpu* pCpu, MemoryBus* pMemory)
	{
		m_pMemoryMapper = pMemoryMapper;
		m_pCpu = pCpu;
		m_pMemory = pMemory;
	}

	void SetTracingEnabled(bool enabled);
	void FlushTrace();

	void OnStart(const char* pRomName);

	void OnPreExecuteOpcode();

	void OnCall(Uint16 unmappedAddress);
	void OnCallInterrupt(Uint16 unmappedAddress);
	void OnReturn(Uint16 unmappedAddress);

	void OnVramAccess(MemoryRequestType requestType, Uint16 address, Uint8 value);
	void OnOamAccess(MemoryRequestType requestType, Uint16 address, Uint8 value);
	
	void OnUnknownOpcode(Uint16 unmappedAddress);

private:
	struct MappedAddress
	{
		Uint8 bank;
		Uint16 address;
		MappedAddress(Uint8 bank_, Uint16 address_) { bank = bank_; address = address_; }
	};

	std::string DebugStringPeek8(Uint16 address);
	std::string DebugStringPeek16(Uint16 address);
	void DebugNextOpcode();

	MappedAddress GetMappedAddress(Uint16 unmappedAddress);

	MemoryMapper* m_pMemoryMapper;
	Cpu* m_pCpu;
	MemoryBus* m_pMemory;
};