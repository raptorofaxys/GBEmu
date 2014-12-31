#pragma once

#include "IMemoryBusDevice.h"

#include "Utils.h"

#include <math.h>

#ifdef NDEBUG
#pragma optimize("", off)
#endif

class Sound : public IMemoryBusDevice
{
public:
	// Implemented loosely following http://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware

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

	//struct SquareWaveGenerator
	//{
	//};

	static const int kWaveRamBase = 0xFF30;
	static const int kWaveRamSize = 0xFF3F - kWaveRamBase + 1;

	static const int kDeviceFrequency = 44100;
	static const int kDeviceNumChannels = 2;
	static const int kDeviceNumBufferSamples = 8192; // below 512, things start to get dicey with xaudio on my hardware
	static const int kDeviceBufferNumMonoSamples = kDeviceNumChannels * kDeviceNumBufferSamples;
	static const int kDeviceBufferByteSize = kDeviceBufferNumMonoSamples * sizeof(Sint16);

	static void AudioCallback(void* userdata, Uint8* pStream8, int numBytes)
	{
		Sint16* pStream16 = reinterpret_cast<Sint16*>(pStream8);
		//const int numSamplesPerChannel = numBytes / sizeof(Sint16) / 2;
		reinterpret_cast<Sound*>(userdata)->FillStreamBuffer(pStream16, numBytes);
	}
	
	Sound()
	{
		if (SDL_GetNumAudioDevices(0) > 0)
		{
			// Get default audio device
			auto deviceName = SDL_GetAudioDeviceName(0, 0);

			SDL_AudioSpec desiredSpec;
			desiredSpec.freq = kDeviceFrequency;
			desiredSpec.format = AUDIO_S16SYS;
			desiredSpec.channels = kDeviceNumChannels;
			desiredSpec.samples = kDeviceNumBufferSamples;
			desiredSpec.callback = &AudioCallback;
			desiredSpec.userdata = this;
			
			SDL_AudioSpec obtainedSpec;

			auto deviceId = SDL_OpenAudioDevice(deviceName, 0, &desiredSpec, &obtainedSpec, 0);
			if (deviceId != 0)
			{
				m_deviceId = deviceId;
				SDL_PauseAudioDevice(deviceId, 0);
			}
		}

		Reset();

		if (m_deviceId != 0)
		{
			SDL_PauseAudioDevice(m_deviceId, 0);
		}
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
		m_sampleTimeLeft = 0.0f;

		//m_ch2Active = false;
		//m_ch2SamplePosition = 0.0f;
		//m_ch2LengthTimer = 0.0f;
		//m_ch2EnvelopeTimer = 0.0f;
	
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

		if (m_deviceId != 0)
		{
			SDL_LockAudioDevice(m_deviceId);
		}

		m_nextBackBufferToTransfer = 0;
		memset(m_backBuffers, 0, sizeof(m_backBuffers));
		// Start in the middle of the second back buffer
		m_numMonoSamplesAvailable = kDeviceBufferNumMonoSamples + kDeviceBufferNumMonoSamples / 2;

		m_audioDeviceActive = false;
		m_masterCounter = 0;
		m_sequencerCounter = 0;

		m_sampleTimeStep = 1.0f / kDeviceFrequency;

		m_ch2FrequencyTimerCounter = 0;
		m_ch2SamplePosition = 0;

		m_ch2LengthCounter = 0;

		m_ch2VolumeCounter = 0;
		m_ch2Volume = 0;

		if (m_deviceId != 0)
		{
			SDL_UnlockAudioDevice(m_deviceId);
		}

		m_tracelogDumpTimer = 0.0f;
		m_traceLog.clear();
	}

	Uint16 GetCh2FrequencyTimerPeriod() const
	{
		return static_cast<Uint16>(((NR24 & 0x7) << 8)) | NR23;
	}

	Uint8 GetCh2VolumeTimerPeriod() const
	{
		return NR22 & 0x7;
	}

	Uint8 GetCh2InitialVolume() const
	{
		return (NR22 & 0xF0) >> 4;
	}

	void OnMasterTick()
	{
		if (m_ch2FrequencyTimerCounter == 0)
		{
			m_ch2FrequencyTimerCounter = (2048 - GetCh2FrequencyTimerPeriod()) * 4;
			m_ch2SamplePosition = (m_ch2SamplePosition + 1) % 8;
		}
		else
		{
			--m_ch2FrequencyTimerCounter;
		}
	}

	void OnLengthTick()
	{
		// Length counter
		if (NR24 & Bit6)
		{
			if (m_ch2LengthCounter > 0)
			{
				--m_ch2LengthCounter;
				if (m_ch2LengthCounter == 0)
				{
					m_ch2Enabled = false;
				}
			}
		}
	}

	void OnVolumeEnvelopeTick()
	{
		if (GetCh2VolumeTimerPeriod() > 0)
		{
			if (m_ch2VolumeCounter > 0)
			{
				--m_ch2VolumeCounter;
				if (m_ch2VolumeCounter == 0)
				{
					if (NR22 & Bit3)
					{
						// Increase volume
						if (m_ch2Volume < 0xF)
						{
							++m_ch2Volume;
						}
					}
					else
					{
						// Decrease volume
						if (m_ch2Volume > 0)
						{
							--m_ch2Volume;
						}
					}

					m_ch2VolumeCounter = GetCh2VolumeTimerPeriod();
				}
			}
		}
	}

	void OnSweepEnvelopeTick()
	{
	}

	void OnSequencerTick()
	{
		if (m_sequencerCounter % 2)
		{
			OnLengthTick();
		}

		if (m_sequencerCounter == 7)
		{
			OnVolumeEnvelopeTick();
		}

		if ((m_sequencerCounter + 2) % 4 == 0)
		{
			OnSweepEnvelopeTick();
		}
	}

	void Update(float seconds)
	{
		if (!m_deviceId)
		{
			return;
		}

		m_updateTimeLeft += seconds;
		m_sampleTimeLeft += seconds;
		m_tracelogDumpTimer += seconds;

		const float timeStep = 1.0f / MemoryBus::kCyclesPerSecond;

		while (m_updateTimeLeft > 0.0f)
		{
			m_masterCounter = (m_masterCounter + 1) % 8192;
			if (m_masterCounter == 0)
			{
				m_sequencerCounter = (m_sequencerCounter + 1) % 8;
				OnSequencerTick();
			}

			OnMasterTick();

			if (!m_audioDeviceActive)
			{
				m_sampleTimeLeft = 0.0f;
			}

			if (m_sampleTimeLeft > 0.0f)
			{
				// Put a sound sample into the backbuffer

				SDL_LockAudioDevice(m_deviceId);

				if (m_numMonoSamplesAvailable < (kDeviceBufferNumMonoSamples * 2))
				{
					Uint16* pCurrentSample = (m_numMonoSamplesAvailable >= kDeviceBufferNumMonoSamples)
						? &m_backBuffers[(m_nextBackBufferToTransfer + 1) % 2][m_numMonoSamplesAvailable - kDeviceBufferNumMonoSamples]
						: &m_backBuffers[m_nextBackBufferToTransfer][m_numMonoSamplesAvailable];

					static Uint8 duties[4][8] =
					{
						{ 0, 0, 0, 0, 0, 0, 0, 1},
						{ 1, 0, 0, 0, 0, 0, 0, 1},
						{ 1, 0, 0, 0, 0, 1, 1, 1},
						{ 0, 1, 1, 1, 1, 1, 1, 0}
					};

					int ch2Duty = (NR21 & 0xC0) >> 6;
					Sint16 ch2Value = duties[ch2Duty][m_ch2SamplePosition] ? m_ch2Volume : -m_ch2Volume;
					ch2Value *= 32767 / 0xF;

					if (!m_ch2Enabled)
					{
						ch2Value = 0;
					}

					//@TODO: mixing, etc.

					// Sine wave test
					//static float f = 0.0f;
					//f += m_sampleTimeStep;
					//ch2Value = sinf(f * 220.0f * 2 * 3.14f) * 4000.0f;

					*pCurrentSample++ = ch2Value;
					*pCurrentSample++ = ch2Value; 

					//printf("Producer: %d mono samples available; adding 2. ", m_numMonoSamplesAvailable);
					m_numMonoSamplesAvailable += 2;
				}
				else
				{
					//printf("Sound device overstuff!  Skipping sample.");
				}

				SDL_UnlockAudioDevice(m_deviceId);

				m_sampleTimeLeft -= m_sampleTimeStep;
			}
			
			m_updateTimeLeft -= timeStep;
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

	void FillStreamBuffer(Sint16* pBuffer, int numBytes)
	{
		SDL_assert(numBytes == kDeviceBufferByteSize);

		m_audioDeviceActive = true;

		if (m_numMonoSamplesAvailable >= kDeviceBufferNumMonoSamples)
		{
			//auto samplesInBuffer = m_numMonoSamplesAvailable - kDeviceBufferNumMonoSamples;
			
			//// If more than 75% full, drop frequency a bit; if less than 25% full, increase a bit.  This accounts for timekeeping drift between the sound card and this program.
			//const float timeStepScale = 1.001f;
			//const int max min
			//if (samplesInBuffer >= (kDeviceBufferNumMonoSamples * 3 / 4))
			//{
			//	m_sampleTimeStep *= timeStepScale;
			//}
			//else if (samplesInBuffer >= (kDeviceBufferNumMonoSamples * 1 / 4))
			//{
			//	m_sampleTimeStep /= timeStepScale;
			//}

			//printf("Consumer: %d mono samples available; taking %d. ", m_numMonoSamplesAvailable, kDeviceBufferNumMonoSamples);
			memcpy(pBuffer, m_backBuffers[m_nextBackBufferToTransfer], kDeviceBufferByteSize);
			m_nextBackBufferToTransfer = (m_nextBackBufferToTransfer + 1) % 2;
			m_numMonoSamplesAvailable -= kDeviceBufferNumMonoSamples;
		}
		else
		{
			//printf("Sound device starvation!\n");
			memset(pBuffer, 0, kDeviceBufferByteSize);
		}

		//static int count = 0;
		//++count;
		//if (count == 4)
		//{
		//	//DebugBreak();
		//}

		//const float timeStep = 1.0f / kDeviceFrequency;
		//const float lengthTimeStep = 1.0f / 256.0f;
		//const float envelopeTimeStep = 1.0f / 64.0f;

		//int ch2Duty = (NR21 & 0xC0) >> 6;
		//int ch2Length = (NR21 & 0x3F);
		//float ch2Frequency = 131072.0f / (2048 - ch2FrequencyValue);
		//int ch2Volume = ((NR22 & 0xF0) >> 4);
		//int ch2EnvelopeStepLength = (NR22 & 0x7);

		//float dummy = 0.0;

		////@TODO: handle this when the value is written to the register
		//if (NR24 & Bit7)
		//{
		//	m_ch2Active = true;
		//	NR24 &= ~Bit7;
		//	ch2Length = 0;
		//}

		////m_ch2Active = true;
		////ch2Length = 0;
		////ch2Duty = 2;
		////ch2Frequency = 220.0f;
		////ch2Volume = 0x3;
		////m_ch2EnvelopeTimer = -999.0f;

		//for (int i = 0; i < numSamplesPerChannel; ++i)
		//{
		//	m_ch2SamplePosition += timeStep * ch2Frequency;
		//	m_ch2SamplePosition = modff(m_ch2SamplePosition, &dummy);

		//	m_ch2LengthTimer += timeStep;
		//	if (m_ch2LengthTimer > 0.0f)
		//	{
		//		m_ch2LengthTimer -= lengthTimeStep;
		//		if ((ch2Length == 63) && ((NR24 & Bit6) != 0))
		//		{
		//			// Stop the sound when the timer expires if the sound is not set to loop forever
		//			m_ch2Active = false;
		//		}
		//		else
		//		{
		//			++ch2Length;
		//			NR21 = (NR21 & 0xC) | ch2Length;
		//		}
		//	}

		//	if (ch2EnvelopeStepLength > 0)
		//	{
		//		m_ch2EnvelopeTimer += (timeStep / ch2EnvelopeStepLength);
		//		if (m_ch2EnvelopeTimer > 0.0f)
		//		{
		//			m_ch2EnvelopeTimer -= envelopeTimeStep;
		//			if (NR22 & Bit3)
		//			{
		//				// Increase volume
		//				if (ch2Volume < 0xF)
		//				{
		//					++ch2Volume;
		//				}
		//			}
		//			else
		//			{
		//				if (ch2Volume > 0)
		//				{
		//					--ch2Volume;
		//				}
		//			}
		//			NR22 = (NR22 & 0x0F) | (ch2Volume << 4);
		//		}
		//	}
		//	else
		//	{
		//		m_ch2EnvelopeTimer = 0.0f;
		//	}


		//	auto sampleIndex = static_cast<int>(m_ch2SamplePosition * ARRAY_SIZE(duties[0]));
		//	Sint16 ch2Value = 0;
		//	if (m_ch2Active)
		//	{
		//		ch2Value = duties[ch2Duty][sampleIndex] ? ch2Volume : -ch2Volume;
		//		//ch2Value = static_cast<int>(sinf(m_ch2SamplePosition * 2 * M_PI) * 32000.0f);
		//		ch2Value *= 32767 / 0xF;
		//	}

		//	static int logSkip = 0;
		//	++logSkip;
		//	if (logSkip % 32 == 0)
		//	{
		//		m_traceLog += Format("%4X %4X %4X %4X\n", 0, ch2Value & 0xFFFF, 0, 0);
		//	}

		//	static float lpf = 0.0f;
		//	static float lpAlpha = 0.9f;
		//	lpf = ((1.0f - lpAlpha) * ch2Value) + (lpAlpha * lpf);

		//	static float hpAlpha = 0.7f;
		//	static float lastLpf = 0.0f;
		//	static float hpf = 0.0f;
		//	hpf = (hpf + (lpf - lastLpf)) * hpAlpha;
		//	lastLpf = lpf;

		//	//*pBuffer = ch2Value;
		//	*pBuffer = static_cast<int>(hpf);
		//	++pBuffer;
		//	//*pBuffer = ch2Value;
		//	*pBuffer = static_cast<int>(hpf);
		//	++pBuffer;
		//}
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

			case Registers::NR21:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR21 | 0x3F;
					}
					else
					{
						NR21 = value;

						m_ch2LengthCounter = 64 - (NR21 & 0x3F);
					}
					return true;
				}
				break;
			case Registers::NR22:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR22 | 0x00;
					}
					else
					{
						NR22 = value;
					}
					return true;
				}
				break;
			case Registers::NR23:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR23 | 0xFF;
					}
					else
					{
						NR23 = value;
					}
					return true;
				}
				break;
			case Registers::NR24:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR24 | 0xBF;
					}
					else
					{
						NR24 = value;

						if (NR24 & Bit7)
						{
							// Channel is now enabled
							m_ch2Enabled = true;
							if (m_ch2LengthCounter == 0)
							{
								m_ch2LengthCounter = 64;
							}
							m_ch2FrequencyTimerCounter = GetCh2FrequencyTimerPeriod();
							m_ch2VolumeCounter = GetCh2VolumeTimerPeriod();
							m_ch2Volume = GetCh2InitialVolume();
							//@TODO: Noise channel's LFSR bits are all set to 1.
							//@TODO: Wave channel's position is set to 0 but sample buffer is NOT refilled.
							//@TODO: Square 1's sweep does several things (see frequency sweep).
						}
					}
					return true;
				}
				break;

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

#ifdef NDEBUG
#pragma optimize("", on)
#endif

private:
	float m_updateTimeLeft;
	float m_sampleTimeLeft;

	SDL_AudioDeviceID m_deviceId;

	bool m_audioDeviceActive;
	Uint16 m_masterCounter;
	Uint16 m_sequencerCounter;
	float m_sampleTimeStep;
	//bool m_ch2Active;
	//float m_ch2SamplePosition;
	//float m_ch2LengthTimer;
	//float m_ch2EnvelopeTimer;

	bool m_ch2Enabled;

	Uint16 m_ch2FrequencyTimerCounter;
	Uint8 m_ch2SamplePosition;
	
	Uint8 m_ch2LengthCounter;
	
	Uint8 m_ch2VolumeCounter;
	Uint8 m_ch2Volume;
	
	//Uint8 m_ch2LengthTimerCounter;

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

	Uint32 m_numMonoSamplesAvailable;
	Uint8 m_nextBackBufferToTransfer;
	Uint16 m_backBuffers[2][kDeviceBufferNumMonoSamples];

	std::string m_traceLog;
	float m_tracelogDumpTimer;
};