#pragma once

#include "IMemoryBusDevice.h"

#include "Utils.h"

#include <math.h>

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

	static const int kDeviceFrequency = 44100;

	static void AudioCallback(void* userdata, Uint8* pStream8, int numBytes)
	{
		Sint16* pStream16 = reinterpret_cast<Sint16*>(pStream8);
		const int numSamplesPerChannel = numBytes / sizeof(Sint16) / 2;
		reinterpret_cast<Sound*>(userdata)->FillStreamBuffer(pStream16, numSamplesPerChannel);
	}
	
	Sound()
	{
		const char* driver_name = SDL_GetCurrentAudioDriver();

		if (driver_name) {
			printf("Audio subsystem initialized; driver = %s.\n", driver_name);
		} else {
			printf("Audio subsystem not initialized.\n");
		}

		for (int i = 0; i < SDL_GetNumAudioDevices(0); ++i)
		{
			printf("Audio device %d: %s\n", i, SDL_GetAudioDeviceName(i, 0));
		}

		if (SDL_GetNumAudioDevices(0) > 0)
		{
			// Get default audio device
			auto deviceName = SDL_GetAudioDeviceName(0, 0);

			SDL_AudioSpec desiredSpec;
			desiredSpec.freq = kDeviceFrequency;
			desiredSpec.format = AUDIO_S16SYS;
			desiredSpec.channels = 2;
			desiredSpec.samples = 512; // below that, with xaudio, things get dicey; //@TODO: generate sound into a back buffer at higher resolution (i.e. take changes to registers into account as soon as they occur)
			desiredSpec.callback = &AudioCallback;
			desiredSpec.userdata = this;
			
			SDL_AudioSpec obtainedSpec;

			auto deviceId = SDL_OpenAudioDevice(deviceName, 0, &desiredSpec, &obtainedSpec, 0);
			if (deviceId != 0)
			{
				m_deviceId = deviceId;
				SDL_PauseAudioDevice(deviceId, 0);
				//SDL_Delay(5000);
				//SDL_CloseAudioDevice(deviceId);
			}
		}

		Reset();
	}

	~Sound()
	{
		if (m_deviceId != 0)
		{
			SDL_CloseAudioDevice(m_deviceId);
		}
	}

	void Reset()
	{
		m_updateTimeLeft = 0.0f;

		m_ch2Active = false;
		m_ch2SamplePosition = 0.0f;
		m_ch2LengthTimer = 0.0f;
		m_ch2EnvelopeTimer = 0.0f;
	
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

		m_tracelogDumpTimer = 0.0f;
		m_traceLog.clear();
	}

	void Update(float seconds)
	{
		m_updateTimeLeft += seconds;
		m_tracelogDumpTimer += seconds;

		const float timeStep = 1.0f / (kDeviceFrequency / 128);

		while (m_updateTimeLeft > 0.0f)
		{
			//int ch2Duty = (NR21 & 0xC) >> 6;
			//int ch2Length = (NR21 & 0x3F);

			//m_updateTimeLeft -= timeStep;
			
			m_updateTimeLeft = 0.0f;
		}

		if (false && (m_deviceId != 0) && (m_tracelogDumpTimer > 0.0f))
		{
			FILE* pFile = nullptr;
			fopen_s(&pFile, "soundlog.txt", "a");

			SDL_LockAudioDevice(m_deviceId);
			fwrite(m_traceLog.data(), m_traceLog.size(), 1, pFile);
			SDL_UnlockAudioDevice(m_deviceId);

			fclose(pFile);

			m_tracelogDumpTimer -= 2.0f;
		}
	}

//#pragma optimize("", off)
	void FillStreamBuffer(Sint16* pBuffer, int numSamplesPerChannel)
	{
		const float timeStep = 1.0f / kDeviceFrequency;
		const float lengthTimeStep = 1.0f / 256.0f;
		const float envelopeTimeStep = 1.0f / 64.0f;

		static Uint8 duties[4][8] =
		{
			{ 1, 0, 0, 0, 0, 0, 0, 0},
			{ 1, 1, 0, 0, 0, 0, 0, 0},
			{ 1, 1, 1, 1, 0, 0, 0, 0},
			{ 1, 1, 1, 1, 1, 1, 0, 0}
		};
		
		int ch2Duty = (NR21 & 0xC) >> 6;
		int ch2Length = (NR21 & 0x3F);
		int ch2FrequencyValue = static_cast<int>(((NR24 & 0x7) << 8)) | NR23;
		float ch2Frequency = 131072.0f / (2048 - ch2FrequencyValue);
		int ch2Volume = ((NR22 & 0xF0) >> 4);
		int ch2EnvelopeStepLength = (NR22 & 0x7);

		float dummy = 0.0;

		//@TODO: handle this when the value is written to the register
		if (NR24 & Bit7)
		{
			m_ch2Active = true;
			NR24 &= ~Bit7;
			ch2Length = 0;
		}

		//m_ch2Active = true;
		//ch2Length = 0;
		//ch2Duty = 2;
		//ch2Frequency = 220.0f;
		//ch2Volume = 0x3;
		//m_ch2EnvelopeTimer = -999.0f;

		for (int i = 0; i < numSamplesPerChannel; ++i)
		{
			m_ch2SamplePosition += timeStep * ch2Frequency;
			m_ch2SamplePosition = modff(m_ch2SamplePosition, &dummy);

			m_ch2LengthTimer += timeStep;
			if (m_ch2LengthTimer > 0.0f)
			{
				m_ch2LengthTimer -= lengthTimeStep;
				if ((ch2Length == 63) && ((NR24 & Bit6) != 0))
				{
					// Stop the sound when the timer expires if the sound is not set to loop forever
					m_ch2Active = false;
				}
				else
				{
					++ch2Length;
					NR21 = (NR21 & 0xC) | ch2Length;
				}
			}

			if (ch2EnvelopeStepLength > 0)
			{
				m_ch2EnvelopeTimer += (timeStep / ch2EnvelopeStepLength);
				if (m_ch2EnvelopeTimer > 0.0f)
				{
					m_ch2EnvelopeTimer -= envelopeTimeStep;
					if (NR22 & Bit3)
					{
						// Increase volume
						if (ch2Volume < 0xF)
						{
							++ch2Volume;
						}
					}
					else
					{
						if (ch2Volume > 0)
						{
							--ch2Volume;
						}
					}
					NR22 = (NR22 & 0x0F) | (ch2Volume << 4);
				}
			}
			else
			{
				m_ch2EnvelopeTimer = 0.0f;
			}


			auto sampleIndex = static_cast<int>(m_ch2SamplePosition * ARRAY_SIZE(duties[0]));
			Sint16 ch2Value = 0;
			if (m_ch2Active)
			{
				ch2Value = duties[ch2Duty][sampleIndex] ? ch2Volume : -ch2Volume;
				//ch2Value = static_cast<int>(sinf(m_ch2SamplePosition * 2 * M_PI) * 32000.0f);
				ch2Value *= 32767 / 0xF;
			}

			static int logSkip = 0;
			++logSkip;
			if (logSkip % 32 == 0)
			{
				m_traceLog += Format("%4X %4X %4X %4X\n", 0, ch2Value & 0xFFFF, 0, 0);
			}

			static float lpf = 0.0f;
			static float lpAlpha = 0.9f;
			lpf = ((1.0f - lpAlpha) * ch2Value) + (lpAlpha * lpf);

			static float hpAlpha = 0.7f;
			static float lastLpf = 0.0f;
			static float hpf = 0.0f;
			hpf = (hpf + (lpf - lastLpf)) * hpAlpha;
			lastLpf = lpf;

			//*pBuffer = ch2Value;
			*pBuffer = static_cast<int>(hpf);
			++pBuffer;
			//*pBuffer = ch2Value;
			*pBuffer = static_cast<int>(hpf);
			++pBuffer;
		}
	}
//#pragma optimize("", on)

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

	SDL_AudioDeviceID m_deviceId;

	bool m_ch2Active;
	float m_ch2SamplePosition;
	float m_ch2LengthTimer;
	float m_ch2EnvelopeTimer;

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

	std::string m_traceLog;
	float m_tracelogDumpTimer;
};