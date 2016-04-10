#include "IMemoryBusDevice.h"

#include "Utils.h"

class UnknownMemoryMappedRegisters: public IMemoryBusDevice
{
	static int const kIoBase = 0xFF00;
	static const int kIoSize = 0xFE7F - kIoBase + 1;
	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value)
	{
		// This device is always at the end of the chain, and it will catch things that fall through everything else
		if (IsAddressInRange(address, kIoBase, kIoSize))
		{
			// Do nothing
			return true;
		}

		return false;
	}
};