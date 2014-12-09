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

	MemoryBus()
	{
		Reset();
	}

	void AddDevice(std::shared_ptr<IMemoryBusDevice> pDevice)
	{
		m_devices.push_back(pDevice);
		m_devicesUnsafe.push_back(pDevice.get());
	}

	void Reset()
	{
		for (auto& index: m_deviceIndexAtAddress)
		{
			index = MemoryDeviceStatus::Unknown;
		}
	}

	Uint8 Read8(Uint16 address, bool throwIfFailed = true, bool* pSuccess = nullptr)
	{
		if (pSuccess)
		{
			*pSuccess = true;
		}


		EnsureDeviceIsProbed(address);
		const auto& deviceIndex = m_deviceIndexAtAddress[address];
		if (deviceIndex >= 0)
		{
			Uint8 result = 0;
			m_devicesUnsafe[deviceIndex]->HandleRequest(MemoryRequestType::Read, address, result);
			return result;
		}

		if (throwIfFailed)
		{
			throw NotImplementedException();
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
		EnsureDeviceIsProbed(address);
		const auto& deviceIndex = m_deviceIndexAtAddress[address];
		if (deviceIndex >= 0)
		{
			m_devicesUnsafe[deviceIndex]->HandleRequest(MemoryRequestType::Write, address, value);
			return;
		}

		throw NotImplementedException();
	}

	void Write16(Uint16 address, Uint16 value)
	{
		Write8(address, GetLow8(value));
		Write8(address + 1, GetHigh8(value));
	}

private:

	static bool breakOnRegisterAccess;
	static Uint16 breakRegister;

	void EnsureDeviceIsProbed(Uint16 address)
	{
		// WARNING: this logic assumes reading is a completely "const" operation, and that it changes the state of the hardware in no way.
		// This is definitely not true on many platforms, but it appears to be the case on GB.  If this assumption does not hold true,
		// we'll have to add another method or perhaps MemoryRequestType to probe the address without altering state.
		int& deviceIndexAtAddress = m_deviceIndexAtAddress[address];
		if (deviceIndexAtAddress == MemoryDeviceStatus::Unknown)
		{
			// very fast
			//if (m_devicesUnsafe[0]->HandleRequest(MemoryRequestType::Read, address, result)) { return result; }
			//if (m_devicesUnsafe[1]->HandleRequest(MemoryRequestType::Read, address, result)) { return result; }

			//for (const auto& pDevice: m_devicesUnsafe) // extremely slow(300-400x slower) in debug, even with iterator debugging turned off
			//for (size_t i = 0; i < m_devicesUnsafe.size(); ++i) // 10-12x slower

			// About the same speed as the range-based for in release
			//@OPTIMIZE: cache which device is used for which address, either on first access or once for all address at the start.  Then prevent calls to AddDevice.
			//@TODO: can even verify that exactly one device handles each address, to make sure there are no overlapping ranges being handled
			//auto end = m_devicesUnsafe.data() + m_devicesUnsafe.size();
			//for (IMemoryBusDevice** ppDevice = m_devicesUnsafe.data(); ppDevice != end; ++ppDevice)
			auto numDevices = m_devicesUnsafe.size();
			for (size_t deviceIndex = 0; deviceIndex < numDevices; ++deviceIndex) // 10-12x slower
			{
				const auto& pDevice = m_devicesUnsafe[deviceIndex];
				Uint8 result;
				if (pDevice->HandleRequest(MemoryRequestType::Read, address, result))
				{
					if (deviceIndexAtAddress == MemoryDeviceStatus::Unknown)
					{
						deviceIndexAtAddress = deviceIndex;
					}
					else
					{
						throw Exception("Two memory devices handle address 0x%04lX", address);
					}
				}
			}
			
			if (deviceIndexAtAddress == MemoryDeviceStatus::Unknown)
			{
				deviceIndexAtAddress = MemoryDeviceStatus::Unset;
			}
		}
	}

	std::vector<std::shared_ptr<IMemoryBusDevice>> m_devices;
	std::vector<IMemoryBusDevice*> m_devicesUnsafe;

	int m_deviceIndexAtAddress[kAddressSpaceSize]; // it's good to be in 2014 - this could be much more efficient in terms of space but there's no need for that right now
};
