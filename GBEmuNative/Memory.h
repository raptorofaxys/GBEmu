#include "IMemoryBusDevice.h"

#include "Utils.h"

class Memory: public IMemoryBusDevice
{
public:
	static const int kWorkMemoryBase = 0xC000;
	static const int kWorkMemorySize = 0x2000; // 8k (not supporting CGB switchable mode)

	// Handling this specially because it overlays memory-mapped registers, but it's just the same as working memory
	static const int kEchoBase = 0xE000;
	static const int kEchoSize = 0xFE00 - kEchoBase;

	static const int kUnusableMemoryBase = 0xFEA0;
	static const int kUnusableMemorySize = 0xFEFF - kUnusableMemoryBase + 1;

	static const int kHramMemoryBase = 0xFF80;
	static const int kHramMemorySize = 0xFFFF - kHramMemoryBase; // last byte is IE register

	Memory()
	{
		Reset();
	}

	void Reset()
	{
		// Initialize to illegal opcode 0xFD
		memset(m_workMemory, 0xFD, sizeof(m_workMemory));
		memset(m_hram, 0xFD, sizeof(m_hram));
	}

private:
	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value)
	{
		if (ServiceMemoryRangeRequest(requestType, address, value, kWorkMemoryBase, kWorkMemorySize, m_workMemory))
		{
			//add logging to see who writes the sprite sheet to 0xC800-...
			return true;
		}
		else if (ServiceMemoryRangeRequest(requestType, address, value, kEchoBase, kEchoSize, m_workMemory))
		{
			return true;
		}
		else if (ServiceMemoryRangeRequest(requestType, address, value, kHramMemoryBase, kHramMemorySize, m_hram))
		{
			return true;
		}
		else if (IsAddressInRange(address, kUnusableMemoryBase, kUnusableMemorySize))
		{
			// Just ignore accesses
			//@TODO: replicate sprite OAM bug
			return true;
		}

		return false;
	}

	Uint8 m_workMemory[kWorkMemorySize];
	Uint8 m_hram[kHramMemorySize];
};