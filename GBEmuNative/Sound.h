#pragma once

#include "IMemoryBusDevice.h"

#include "Utils.h"

class Sound : public IMemoryBusDevice
{
public:
	enum class Registers
	{
		NR10 = 0xFF10, 	// Sound: channel 1 sweep register
		NR11 = 0xFF11, 	// Sound: channel 1 sound length/wave pattern duty
		NR12 = 0xFF12, 	// Sound: channel 1 volume envelope
		NR13 = 0xFF13, 	// Sound: channel 1 frequency low
		NR14 = 0xFF14, 	// Sound: channel 1 frequency high

		NR21 = 0xFF16,	// Sound: channel 2 sound length/wave pattern duty
		NR22 = 0xFF17,	// Sound: channel 2 volume envelope
		NR23 = 0xFF18,	// Sound: channel 2 frequency low
		NR24 = 0xFF19,	// Sound: channel 2 frequency high

		NR30 = 0xFF1A,	// Sound: channel 3 sound on/off
		NR31 = 0xFF1B,	// Sound: channel 3 sound length
		NR32 = 0xFF1C,	// Sound: channel 3 select output level
		NR33 = 0xFF1D,	// Sound: channel 3 frequency low
		NR34 = 0xFF1E,	// Sound: channel 3 frequency high

		NR41 = 0xFF20,	// Sound: channel 4 sound length
		NR42 = 0xFF21,	// Sound: channel 4 volume envelope
		NR43 = 0xFF22,	// Sound: channel 4 polynomial counter
		NR44 = 0xFF23,	// Sound: channel 4 counter/consecutive; initial

		NR50 = 0xFF24, 	// Sound: channel control, on/off, volume
		NR51 = 0xFF25, 	// Sound: selection of sound output terminal
		NR52 = 0xFF26, 	// Sound: sound on/off
	};

	static const int kWaveRamBase = 0xFF30;
	static const int kWaveRamSize = 0xFF3F - kWaveRamBase + 1;

	Sound()
	{
		Reset();
	}

	void Reset()
	{
		m_updateTimeLeft = 0.0f;

		NR10 = 0x80;
		NR11 = 0xBF;
		NR12 = 0xF3;
		NR13 = 0x00;
		NR14 = 0xBF;

		NR21 = 0x3F;
		NR22 = 0x00;
		NR23 = 0x00;
		NR24 = 0xBF;

		NR30 = 0x7F;
		NR31 = 0xFF;
		NR32 = 0x9F;
		NR33 = 0xBF;
		NR34 = 0x00;

		NR41 = 0xFF;
		NR42 = 0x00;
		NR43 = 0x00;
		NR44 = 0xBF;

		NR50 = 0x77;
		NR51 = 0xF3;
		NR52 = 0xF1;
	}

	void Update(float seconds)
	{
		m_updateTimeLeft += seconds;

		while (m_updateTimeLeft > 0.0f)
		{
			//@TODO
			m_updateTimeLeft = 0.0f;
		}
	}

	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value)
	{
		if (ServiceMemoryRangeRequest(requestType, address, value, kWaveRamBase, kWaveRamSize, m_waveRam))
		{
			return true;
		}
		else
		{
			switch (address)
			{
			SERVICE_MMR_RW(NR10)
			SERVICE_MMR_RW(NR11)
			SERVICE_MMR_RW(NR12)
			SERVICE_MMR_RW(NR13)
			SERVICE_MMR_RW(NR14)

			SERVICE_MMR_RW(NR21)
			SERVICE_MMR_RW(NR22)
			SERVICE_MMR_RW(NR23)
			SERVICE_MMR_RW(NR24)

			SERVICE_MMR_RW(NR30)
			SERVICE_MMR_RW(NR31)
			SERVICE_MMR_RW(NR32)
			SERVICE_MMR_RW(NR33)
			SERVICE_MMR_RW(NR34)

			SERVICE_MMR_RW(NR41)
			SERVICE_MMR_RW(NR42)
			SERVICE_MMR_RW(NR43)
			SERVICE_MMR_RW(NR44)

			SERVICE_MMR_RW(NR50)
			SERVICE_MMR_RW(NR51)
			SERVICE_MMR_RW(NR52)
			}
		}
	
		return false;
	}
private:
	float m_updateTimeLeft;

	Uint8 NR10;
	Uint8 NR11;
	Uint8 NR12;
	Uint8 NR13;
	Uint8 NR14;

	Uint8 NR21;
	Uint8 NR22;
	Uint8 NR23;
	Uint8 NR24;

	Uint8 NR30;
	Uint8 NR31;
	Uint8 NR32;
	Uint8 NR33;
	Uint8 NR34;

	Uint8 NR41;
	Uint8 NR42;
	Uint8 NR43;
	Uint8 NR44;

	Uint8 NR50;
	Uint8 NR51;
	Uint8 NR52;

	Uint8 m_waveRam[kWaveRamSize];
};