#pragma once

#include "IMemoryBusDevice.h"

class MemoryMapper : public IMemoryBusDevice
{
public:
	virtual void Reset() = 0;
};