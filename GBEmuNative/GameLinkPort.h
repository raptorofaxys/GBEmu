#pragma once

#include "IMemoryBusDevice.h"

#include "Utils.h"

class GameLinkPort : public IMemoryBusDevice
{
public:
	enum class Registers
	{
		SB = 0xFF01,	// Serial transfer data
		SC = 0xFF02,	// Serial transfer control
	};

	GameLinkPort(const std::shared_ptr<Cpu>& cpu)
	{
        m_pCpu = cpu;
		Reset();
	}

	void Reset()
	{
		SB = 0;
		SC = 0;
        m_updateTimeLeft = 0.0f;
        m_pendingOutboundTransfer = false;
	}

    void Update(float seconds)
    {
        m_updateTimeLeft += seconds;
        if (m_pendingOutboundTransfer)
        {
            if (m_updateTimeLeft >= 8.0f / 8192)
            {
                m_pendingOutboundTransfer = false;
                m_pCpu->SignalInterrupt(Bit3);
            }
        }
        else
        {
            m_updateTimeLeft = 0.0f;
        }
    }

	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value)
	{
		switch (address)
		{
		case Registers::SC:
			{
				if (requestType == MemoryRequestType::Read)
				{
					value = SC;
				}
				else
				{
					SC = value;
					
					//@TODO: timing emulation, internal/external clock, interrupt
					if (value & Bit7)
					{
						//printf("Serial output byte: %c (0x%02lX)\n", m_SB, m_SB);
						printf("%c", SB);
						SC &= ~Bit7;
						// Read 0xFF, which is the default value in actual hardware
						SB = 0xFF;
                        
                        m_pendingOutboundTransfer = true;
					}
				}
				return true;
			}

		SERVICE_MMR_RW(SB)
		}
	
		return false;
	}
private:
	Uint8 SB;
	Uint8 SC;
    
    float m_updateTimeLeft;
    bool m_pendingOutboundTransfer;

    std::shared_ptr<Cpu> m_pCpu;
};