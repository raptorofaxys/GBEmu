#include "IMemoryBusDevice.h"

#include "Utils.h"

class UnknownMemoryMappedRegisters: public IMemoryBusDevice
{
	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value)
	{
		switch (address)
		{
		case 0xFF7B:
		case 0xFF7C:
		case 0xFF7D:
		case 0xFF7E:
		case 0xFF7F:
			// Just ignore accesses
			return true;
		}

		return false;
	}
};