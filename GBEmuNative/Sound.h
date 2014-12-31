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
	// Implemented loosely following http://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware.  There are many strange behaviours
	// in the DMG hardware; this code is commented very loosely, and readers should refer to the above page for further details.

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

	class SquareWaveGenerator
	{
	public:
		SquareWaveGenerator(const Uint8& NRx1, const Uint8& NRx3, const Uint8& NRx4)
			: m_NRx1(NRx1)
			, m_NRx3(NRx3)
			, m_NRx4(NRx4)
		{
			Reset();
		}

		Uint16 GetFrequencyTimerPeriod() const
		{
			// MSB are in NRx4, LSB are in NRx3
			return static_cast<Uint16>(((m_NRx4 & 0x7) << 8)) | m_NRx3;
		}

		void Reset()
		{
			m_frequencyTimerCounter = GetFrequencyTimerPeriod();
			m_samplePosition = 0;
		}

		void Tick()
		{
			if (m_frequencyTimerCounter == 0)
			{
				m_frequencyTimerCounter = (2048 - GetFrequencyTimerPeriod()) * 4;
				
				// The generator has eight steps
				m_samplePosition = (m_samplePosition + 1) % 8;
			}
			else
			{
				--m_frequencyTimerCounter;
			}
		}

		Sint16 GetOutput() const
		{
			static Uint8 duties[4][8] =
			{
				{ 0, 0, 0, 0, 0, 0, 0, 1},
				{ 1, 0, 0, 0, 0, 0, 0, 1},
				{ 1, 0, 0, 0, 0, 1, 1, 1},
				{ 0, 1, 1, 1, 1, 1, 1, 0}
			};

			int duty = (m_NRx1 & 0xC0) >> 6;

			return duties[duty][m_samplePosition] ? 1 : -1;
		}
		
	private:
		const Uint8& m_NRx1;
		const Uint8& m_NRx3;
		const Uint8& m_NRx4;

		Uint16 m_frequencyTimerCounter;
		Uint8 m_samplePosition;
	};

	class LengthCounter
	{
	public:
		LengthCounter(const Uint8& NRx1, const Uint8& NRx4)
			: m_NRx1(NRx1)
			, m_NRx4(NRx4)
		{
			ResetLength();
		}

		void ResetLength()
		{
			m_ch2LengthCounter = 64 - (m_NRx1 & 0x3F);
		}

		void Enable()
		{
			m_enabled = true;
			if (m_ch2LengthCounter == 0)
			{
				m_ch2LengthCounter = 64;
			}
		}

		bool IsChannelEnabled() const
		{
			return m_enabled;
		}

		void Tick()
		{
			if (m_NRx4 & Bit6)
			{
				if (m_ch2LengthCounter > 0)
				{
					--m_ch2LengthCounter;
					if (m_ch2LengthCounter == 0)
					{
						// If the length has expired, turn the channel off
						m_enabled = false;
					}
				}
			}
		}

	private:
		const Uint8& m_NRx1;
		const Uint8& m_NRx4;

		bool m_enabled;
		Uint8 m_ch2LengthCounter;
	};
	
	class VolumeEnvelope
	{
	public:
		VolumeEnvelope(const Uint8& NRx2)
			: m_NRx2(NRx2)
		{
		}

		Uint8 GetVolumeTimerPeriod() const
		{
			return m_NRx2 & 0x7;
		}

		Uint8 GetInitialVolume() const
		{
			return (m_NRx2 & 0xF0) >> 4;
		}

		void Reset()
		{
			m_volumeCounter = GetVolumeTimerPeriod();
			m_volume = GetInitialVolume();
		}

		Uint8 GetVolume() const
		{
			return m_volume;
		}

		void Tick()
		{
			if (GetVolumeTimerPeriod() > 0)
			{
				if (m_volumeCounter > 0)
				{
					--m_volumeCounter;
					if (m_volumeCounter == 0)
					{
						if (m_NRx2 & Bit3)
						{
							// Increase volume
							if (m_volume < 0xF)
							{
								++m_volume;
							}
						}
						else
						{
							// Decrease volume
							if (m_volume > 0)
							{
								--m_volume;
							}
						}

						m_volumeCounter = GetVolumeTimerPeriod();
					}
				}
			}
		}

	private:
		const Uint8& m_NRx2;

		Uint8 m_volumeCounter;
		Uint8 m_volume;
	};

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
		reinterpret_cast<Sound*>(userdata)->FillStreamBuffer(pStream16, numBytes);
	}
	
	Sound()
		: m_ch2Generator(NR21, NR23, NR24)
		, m_ch2LengthCounter(NR21, NR24)
		, m_ch2VolumeEnvelope(NR22)
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

		m_ch2Generator.Reset();
		m_ch2LengthCounter.ResetLength();
		m_ch2VolumeEnvelope.Reset();

		if (m_deviceId != 0)
		{
			SDL_UnlockAudioDevice(m_deviceId);
		}

		m_tracelogDumpTimer = 0.0f;
		m_traceLog.clear();
	}

	void OnMasterTick()
	{
		m_ch2Generator.Tick();
	}

	void OnLengthTick()
	{
		m_ch2LengthCounter.Tick();
	}

	void OnVolumeEnvelopeTick()
	{
		m_ch2VolumeEnvelope.Tick();
	}

	void OnSweepEnvelopeTick()
	{
	}

	void OnSequencerTick()
	{
		// Emulate the tick sequence as per the hardware docs
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

					Sint16 ch2Value = m_ch2Generator.GetOutput() * m_ch2VolumeEnvelope.GetVolume();
					ch2Value *= 32767 / 0xF;

					if (!m_ch2LengthCounter.IsChannelEnabled())
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

						m_ch2LengthCounter.ResetLength();
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

							m_ch2Generator.Reset();
							m_ch2LengthCounter.Enable();
							m_ch2VolumeEnvelope.Reset();
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

	SquareWaveGenerator m_ch2Generator;
	LengthCounter m_ch2LengthCounter;
	VolumeEnvelope m_ch2VolumeEnvelope;
	
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