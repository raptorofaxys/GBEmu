#pragma once

#include "IMemoryBusDevice.h"

class MemoryMapper : public IMemoryBusDevice
{
public:
	virtual void Reset() = 0;
	virtual Uint8 GetActiveBank() = 0;
};