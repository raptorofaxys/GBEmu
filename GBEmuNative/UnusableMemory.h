#include "IMemoryBusDevice.h"

#include "Utils.h"

class UnusableMemory: public IMemoryBusDevice
{
	static const int kUnusableMemoryBase = 0xFEA0;
	static const int kUnusableMemorySize = 0xFEFF - kUnusableMemoryBase + 1;

	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value)
	{
		if (IsAddressInRange(address, kUnusableMemoryBase, kUnusableMemorySize))
		{
			// Just ignore accesses
			//@TODO: replicate sprite OAM bug
			return true;
		}

		return false;
	}
};