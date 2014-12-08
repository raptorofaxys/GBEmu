#include "IMemoryBusDevice.h"

#include "Utils.h"

class UnknownMemoryMappedRegisters: public IMemoryBusDevice
{
	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value)
	{
		switch (address)
		{
		case 0xFF7F:
			// Tetris writes here, no documented function
			// Just ignore accesses
			return true;
		}

		return false;
	}
};