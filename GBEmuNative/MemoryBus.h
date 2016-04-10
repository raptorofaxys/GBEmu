#pragma once

#include "IMemoryBusDevice.h"
#include "Utils.h"

#include "SDL.h"

#include <memory>
#include <vector>

extern float g_totalCyclesExecuted;

namespace MemoryDeviceStatus
{
	enum Type
	{
		Unknown = -2,
		Unset = -1
	};
}

class MemoryBus
{
public:

	static const int kAddressSpaceSize = 0x10000;

	static Uint32 const kCyclesPerSecond = 4194304;


	MemoryBus()
	{
		Reset();
		m_devicesLocked = false;
	}

	void AddDevice(std::shared_ptr<IMemoryBusDevice> pDevice)
	{
		SDL_assert(!m_devicesLocked);

		m_devices.push_back(pDevice);
		m_devicesUnsafe.push_back(pDevice.get());
	}

	void LockDevices(Analyzer* pAnalyzer)
	{
		m_pAnalyzer = pAnalyzer;
		for (auto& device : m_devicesUnsafe)
		{
			device->SetAnalyzer(pAnalyzer);
		}

		for (Uint32 address = 0; address < kAddressSpaceSize; ++address)
		{
			m_deviceIndexAtAddress[address] = MemoryDeviceStatus::Unknown;
			EnsureDeviceIsProbed(address);
		}
		m_devicesLocked = true;
	}

	void Reset()
	{
	}

	Uint8 Read8(Uint16 address, bool throwIfFailed = true, bool* pSuccess = nullptr)
	{
		if (pSuccess)
		{
			*pSuccess = true;
		}

		if (dataBreakpointActive && (address == dataBreakpointAddress))
		{
			static volatile int x = 3;
			++x;
		}

		SDL_assert(m_devicesLocked);
		const auto& deviceIndex = m_deviceIndexAtAddress[address];
		if (deviceIndex >= 0)
		{
			Uint8 result = 0;
			m_devicesUnsafe[deviceIndex]->HandleRequest(MemoryRequestType::Read, address, result);
			m_pAnalyzer->OnPostRead8(address, result);
			return result;
		}

		if (throwIfFailed)
		{
			throw Exception("Attempted read of at address 0x%04lX.", address);
		}
		if (pSuccess)
		{
			*pSuccess = false;
		}
		return 0xFF;
	}
	
	bool SafeRead8(Uint16 address, Uint8& value)
	{
		bool success = true;
		value = Read8(address, false, &success);
		return success;
	}

	Uint16 Read16(Uint16 address)
	{
		// Must handle as two reads, because the address can cross range boundaries
		return Make16(Read8(address + 1), Read8(address));
	}

	void Write8(Uint16 address, Uint8 value)
	{
		if (dataBreakpointActive && (address == dataBreakpointAddress))
		{
			static volatile int x = 3;
			++x;
		}

		SDL_assert(m_devicesLocked);
		const auto& deviceIndex = m_deviceIndexAtAddress[address];
		if (deviceIndex >= 0)
		{
			m_devicesUnsafe[deviceIndex]->HandleRequest(MemoryRequestType::Write, address, value);
			m_pAnalyzer->OnPostWrite8(address, value);
			return;
		}

		throw Exception("Attempted write of value %d at address 0x%04lX.", value, address);
	}

	void Write16(Uint16 address, Uint16 value)
	{
		Write8(address, GetLow8(value));
		Write8(address + 1, GetHigh8(value));
	}

private:

	static bool dataBreakpointActive;
	static Uint16 dataBreakpointAddress;

	void EnsureDeviceIsProbed(Uint16 address)
	{
		// WARNING: this logic assumes reading is a completely "const" operation, and that it changes the state of the hardware in no way.
		// This is definitely not true on many platforms, but it appears to be the case on GB.  If this assumption does not hold true,
		// we'll have to add another method or perhaps MemoryRequestType to probe the address without altering state.
		auto& deviceIndexAtAddress = m_deviceIndexAtAddress[address];
		if (deviceIndexAtAddress == MemoryDeviceStatus::Unknown)
		{
			// We want to cache which device services requests for a given memory address, because scanning through the devices each time an access occurs is very slow.
			// very fast
			//if (m_devicesUnsafe[0]->HandleRequest(MemoryRequestType::Read, address, result)) { return result; }
			//if (m_devicesUnsafe[1]->HandleRequest(MemoryRequestType::Read, address, result)) { return result; }

			//for (const auto& pDevice: m_devicesUnsafe) // extremely slow(300-400x slower) in debug, even with iterator debugging turned off
			//for (size_t i = 0; i < m_devicesUnsafe.size(); ++i) // 10-12x slower

			// About the same speed as the range-based for in release
			//auto end = m_devicesUnsafe.data() + m_devicesUnsafe.size();
			//for (IMemoryBusDevice** ppDevice = m_devicesUnsafe.data(); ppDevice != end; ++ppDevice)
			auto numDevices = static_cast<Sint8>(m_devicesUnsafe.size());
			SDL_assert(numDevices > 0);
			for (Sint8 deviceIndex = 0; deviceIndex < numDevices; ++deviceIndex) // 10-12x slower
			{
				const auto& pDevice = m_devicesUnsafe[deviceIndex];
				Uint8 result;
				if (pDevice->HandleRequest(MemoryRequestType::Read, address, result))
				{
					deviceIndexAtAddress = deviceIndex;
					break;
				}
			}
			
			if (deviceIndexAtAddress == MemoryDeviceStatus::Unknown)
			{
				deviceIndexAtAddress = MemoryDeviceStatus::Unset;
			}
		}
	}

	Analyzer* m_pAnalyzer;

	bool m_devicesLocked;
	std::vector<std::shared_ptr<IMemoryBusDevice>> m_devices;
	std::vector<IMemoryBusDevice*> m_devicesUnsafe;

	Sint8 m_deviceIndexAtAddress[kAddressSpaceSize]; // it's good to be in 2014(2015(2016)) - this could be much more efficient in terms of space but there's no need for that right now
};
