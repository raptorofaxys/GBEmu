#pragma once

#include "IMemoryBusDevice.h"

#include "Utils.h"

#include <math.h>

#ifdef NDEBUG
#pragma optimize("", off)
#endif

//#define FORCENOINLINE __declspec(noinline)

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

	class LengthCounter
	{
	public:
		LengthCounter(const Uint8& NRx1, const Uint8& NRx4, bool eightBitLengthMode)
			: m_NRx1(NRx1)
			, m_NRx4(NRx4)
			, m_eightBitLengthMode(eightBitLengthMode)
		{
			m_enabled = false;
			ResetLength();
		}

		void ResetLength()
		{
			m_lengthCounter = m_eightBitLengthMode ? m_NRx1 : (64 - (m_NRx1 & 0x3F));
			UpdateEnabledStatusFromLength();
		}

		void Enable()
		{
			m_enabled = true;
			if (m_lengthCounter == 0)
			{
				m_lengthCounter = m_eightBitLengthMode ? 256 : 64;
			}
		}

		void Disable()
		{
			m_enabled = false;
		}

		bool IsChannelEnabled() const
		{
			return m_enabled;
		}

		void Tick()
		{
			if (m_NRx4 & Bit6)
			{
				if (m_lengthCounter > 0)
				{
					--m_lengthCounter;
					UpdateEnabledStatusFromLength();
				}
			}
		}

		void UpdateEnabledStatusFromLength()
		{
			if (m_lengthCounter == 0)
			{
				// If the length has expired, turn the channel off
				m_enabled = false;
			}
		}

	private:
		const Uint8& m_NRx1;
		const Uint8& m_NRx4;

		bool m_enabled;
		bool m_eightBitLengthMode;
		Uint16 m_lengthCounter;
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

	class FrequencySweep
	{
	public:
		// This unit really is super strange according to the docs... lots of little strange corner cases.  Used both http://problemkaputt.de/pandocs.htm and http://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware to cobble something together that sounded sort of OK.
		FrequencySweep(const Uint8& NRx0, Uint8& NRx3, Uint8& NRx4, LengthCounter& lengthCounter)
			: m_NRx0(NRx0)
			, m_NRx3(NRx3)
			, m_NRx4(NRx4)
			, m_lengthCounter(lengthCounter)
		{
			Reset();
		}

		void Reset()
		{
			m_shadowFrequency = GetCurrentFrequency();
			m_sweepTimer = GetSweepTimerPeriod();
			m_enabled = (GetSweepTimerPeriod() != 0) && (GetSweepShift() != 0);
			if (GetSweepShift() != 0)
			{
				ComputeNextFrequencyAndCheckForOverflow();
			}
		}

		Uint16 GetCurrentFrequency() const
		{
			// MSB are in NRx4, LSB are in NRx3
			return static_cast<Uint16>(((m_NRx4 & 0x7) << 8)) | m_NRx3;
		}

		void SetCurrentFrequency(Uint16 frequency) const
		{
			SDL_assert(frequency <= 2047);
			Uint8 lsb = frequency & 0xFF;
			Uint8 msb = (frequency >> 8) & 0x7;
			// MSB are in NRx4, LSB are in NRx3
			m_NRx3 = lsb;
			m_NRx4 = (m_NRx4 & ~0x7) | msb;

			if (GetCurrentFrequency() != frequency)
			{
				DebugBreak();
			}
		}

		Uint8 GetSweepTimerPeriod() const
		{
			Uint8 result = (m_NRx0 & 0x70) >> 4;
			if (result == 0)
			{
				result = 8;
			}
			return result;
		}

		Uint8 GetSweepShift() const
		{
			return (m_NRx0 & 0x7);
		}

		Sint16 GetSweepDirectionMultiplier() const
		{
			return ((m_NRx0 & Bit3) != 0) ? -1 : 1;
		}


		Uint16 ComputeNextFrequency() const
		{
			if (GetSweepShift() == 0)
			{
				return m_shadowFrequency;
			}

			Sint16 delta = m_shadowFrequency >> GetSweepShift();
			delta *= GetSweepDirectionMultiplier();
			Uint16 result = m_shadowFrequency + delta;
			return result;
		}

		void PerformOverflowCheck(Uint16 frequency) const
		{
			if (frequency > 2047)
			{
				m_lengthCounter.Disable();
			}
		}

		void ComputeNextFrequencyAndCheckForOverflow()
		{
			Uint16 nextFrequency = ComputeNextFrequency();
			PerformOverflowCheck(nextFrequency);
		}

		void Tick()
		{
			if (!m_enabled)
			{
				return;
			}

			if (m_sweepTimer > 0)
			{
				--m_sweepTimer;
				if (m_sweepTimer == 0)
				{
					m_sweepTimer = GetSweepTimerPeriod();
					Uint16 nextFrequency = ComputeNextFrequency();
					if (nextFrequency <= 2047)
					{
						m_shadowFrequency = nextFrequency;
						SetCurrentFrequency(m_shadowFrequency);
					}
					ComputeNextFrequencyAndCheckForOverflow();
				}
			}
		}

	private:
		const Uint8& m_NRx0;
		Uint8& m_NRx3;
		Uint8& m_NRx4;
		LengthCounter& m_lengthCounter;

		Uint16 m_shadowFrequency;
		Uint16 m_sweepTimer;
		bool m_enabled;
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

		Uint16 GetDefaultFrequency() const
		{
			// MSB are in NRx4, LSB are in NRx3
			return static_cast<Uint16>(((m_NRx4 & 0x7) << 8)) | m_NRx3;
		}

		//Uint16 GetFrequency()
		//{
		//	return m_frequency;
		//}

		//void SetFrequency(Uint16 frequency)
		//{
		//	m_frequency = frequency;
		//}

		Uint16 GetTimerPeriodFromFrequency(Uint16 frequency) const
		{
			return (2048 - frequency) * 4;
		}

		void ResetTimerPeriodFromFrequency()
		{
			m_frequencyTimerCounter = GetTimerPeriodFromFrequency(GetDefaultFrequency());
		}

		void Reset()
		{
			//m_frequency = GetDefaultFrequency();
			ResetTimerPeriodFromFrequency();
			m_samplePosition = 0;
		}

		void Tick()
		{
			SDL_assert(m_frequencyTimerCounter > 0);
			--m_frequencyTimerCounter;
			if (m_frequencyTimerCounter == 0)
			{
				ResetTimerPeriodFromFrequency();
				
				// The generator has eight steps
				m_samplePosition = (m_samplePosition + 1) % 8;
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

		//Uint16 m_frequency; // 0 to 2047
		Uint16 m_frequencyTimerCounter;
		Uint8 m_samplePosition;
	};
	
	class NoiseGenerator
	{
	public:
		NoiseGenerator(const Uint8& NRx3)
			: m_NRx3(NRx3)
		{
			Reset();
		}

		Uint8 GetClockShift() const
		{
			return (m_NRx3 & 0xF0) >> 4;
		}

		bool IsSevenBitMode() const
		{
			return (m_NRx3 & Bit3) != 0;
		}

		Uint8 GetDivisorCode() const
		{
			return m_NRx3 & 0x7;
		}

		Uint16 GetTimerPeriod() const
		{
			static Uint16 baseDivisors[] = {8, 16, 32, 48, 64, 80, 96, 112};
			return baseDivisors[GetDivisorCode()] << GetClockShift();
		}

		void ResetTimerPeriodFromFrequency()
		{
			m_frequencyTimerCounter = GetTimerPeriod();
		}

		void Reset()
		{
			ResetTimerPeriodFromFrequency();
			m_lfsr = 0xFFFF;
			//m_lfsr = rand();
		}

		void Tick()
		{
			--m_frequencyTimerCounter;
			if (m_frequencyTimerCounter == 0)
			{
				ResetTimerPeriodFromFrequency();
				
				// Documented implementation sounds further from the hardware than just a call to rand()
				Uint16 xor01 = (m_lfsr & Bit0) ^ ((m_lfsr & Bit1) >> 1);
				m_lfsr >>= 1;
				m_lfsr |= xor01 << 15;
				if (IsSevenBitMode())
				{
					m_lfsr |= xor01 << 6;
				}

				// Crush the above with a simple pseudorandom call - at high enough frequencies this sounds very similar to actual hardware... not true to the hardware, but it sounds better than the above
				m_lfsr = rand() % 2;
			}
		}

		Sint16 GetOutput() const
		{
			return 1 ^ (m_lfsr & Bit0);
		}
		
	private:
		const Uint8& m_NRx3;

		Uint16 m_lfsr;
		Uint16 m_frequencyTimerCounter;
	};

	class WavetableGenerator
	{
	public:
		WavetableGenerator(const Uint8& NRx0, const Uint8& NRx2, const Uint8& NRx3, const Uint8& NRx4, const Uint8* pWaveRam)
			: m_NRx0(NRx0)
			, m_NRx2(NRx2)
			, m_NRx3(NRx3)
			, m_NRx4(NRx4)
			, m_pWaveRam(pWaveRam)
		{
			m_output = 0; // Not part of Reset
			Reset();
		}

		bool IsEnabled() const
		{
			return (m_NRx0 & Bit7) != 0;
		}

		Uint16 GetDefaultFrequency() const
		{
			// MSB are in NRx4, LSB are in NRx3
			return static_cast<Uint16>(((m_NRx4 & 0x7) << 8)) | m_NRx3;
		}

		Uint8 GetVolumeShift() const
		{
			Uint8 code = (m_NRx2 >> 5) & 0x3;
			static Uint8 volumeShifts[] = {4, 0, 1, 2};
			return volumeShifts[code];
		}

		Uint16 GetTimerPeriodFromFrequency(Uint16 frequency) const
		{
			return (2048 - frequency) * 2;
		}

		void ResetTimerPeriodFromFrequency()
		{
			m_frequencyTimerCounter = GetTimerPeriodFromFrequency(GetDefaultFrequency());
		}

		void Reset()
		{
			ResetTimerPeriodFromFrequency();
			m_samplePosition = 0;
		}

		void Tick()
		{
			SDL_assert(m_frequencyTimerCounter > 0);
			--m_frequencyTimerCounter;
			if (m_frequencyTimerCounter == 0)
			{
				ResetTimerPeriodFromFrequency();

				m_samplePosition = (m_samplePosition + 1) % 32;
				Uint8 sampleIndex = m_samplePosition >> 1;
				Uint8 sample = ((m_samplePosition & 1) != 0) ? (m_pWaveRam[sampleIndex] & 0x0F) : ((m_pWaveRam[sampleIndex] >> 4) & 0xF);
				m_output = sample >> GetVolumeShift(); // 0-15
				m_output = -8192 + (m_output * (16384 / 15));
			}
		}

		Sint16 GetOutput() const
		{
			return IsEnabled() ? m_output : 0;
		}
		
	private:
		const Uint8& m_NRx0;
		const Uint8& m_NRx2;
		const Uint8& m_NRx3;
		const Uint8& m_NRx4;

		Uint16 m_frequencyTimerCounter;
		Uint8 m_samplePosition;
		Sint16 m_output;
		const Uint8* m_pWaveRam;
	};
	
	static const int kWaveRamBase = 0xFF30;
	static const int kWaveRamSize = 0xFF3F - kWaveRamBase + 1;

	static const int kDeviceFrequency = 44100;
	static const int kDeviceNumChannels = 2;
	static const int kDeviceNumBufferSamples = 1024; // below 1024, things start to get dicey with xaudio on my hardware
	static const int kDeviceBufferNumMonoSamples = kDeviceNumChannels * kDeviceNumBufferSamples;
	static const int kDeviceBufferByteSize = kDeviceBufferNumMonoSamples * sizeof(Sint16);

	static void AudioCallback(void* userdata, Uint8* pStream8, int numBytes)
	{
		Sint16* pStream16 = reinterpret_cast<Sint16*>(pStream8);
		reinterpret_cast<Sound*>(userdata)->FillStreamBuffer(pStream16, numBytes);
	}
	
	Sound()
		: m_ch1Sweep(NR10, NR13, NR14, m_ch1LengthCounter)
		, m_ch1Generator(NR11, NR13, NR14)
		, m_ch1LengthCounter(NR11, NR14, false)
		, m_ch1VolumeEnvelope(NR12)
		, m_ch2Generator(NR21, NR23, NR24)
		, m_ch2LengthCounter(NR21, NR24, false)
		, m_ch2VolumeEnvelope(NR22)
		, m_ch3Generator(NR30, NR32, NR33, NR34, m_waveRam)
		, m_ch3LengthCounter(NR31, NR34, true)
		, m_ch4Generator(NR43)
		, m_ch4LengthCounter(NR41, NR44, false)
		, m_ch4VolumeEnvelope(NR42)
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

		m_ch1Generator.Reset();
		m_ch1LengthCounter.ResetLength();
		m_ch1VolumeEnvelope.Reset();

		m_ch2Generator.Reset();
		m_ch2LengthCounter.ResetLength();
		m_ch2VolumeEnvelope.Reset();

		m_ch3Generator.Reset();
		m_ch3LengthCounter.ResetLength();

		m_ch4Generator.Reset();
		m_ch4LengthCounter.ResetLength();
		m_ch4VolumeEnvelope.Reset();

		if (m_deviceId != 0)
		{
			SDL_UnlockAudioDevice(m_deviceId);
		}

		m_tracelogDumpTimer = 0.0f;
		m_traceLog.clear();
	}

	void OnMasterTick()
	{
		m_ch1Generator.Tick();
		m_ch2Generator.Tick();
		m_ch3Generator.Tick();
		m_ch4Generator.Tick();
	}

	void OnLengthTick()
	{
		m_ch1LengthCounter.Tick();
		m_ch2LengthCounter.Tick();
		m_ch3LengthCounter.Tick();
		m_ch4LengthCounter.Tick();
	}

	void OnVolumeEnvelopeTick()
	{
		m_ch1VolumeEnvelope.Tick();
		m_ch2VolumeEnvelope.Tick();
		m_ch4VolumeEnvelope.Tick();
	}

	void OnSweepEnvelopeTick()
	{
		m_ch1Sweep.Tick();
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

					auto ComputeChannelOutput = [&](Uint16 generatorOutput, LengthCounter l, VolumeEnvelope v)
						{ 
							Sint16 value = generatorOutput * v.GetVolume();
							value *= 8191 / 0xF;

							if (!l.IsChannelEnabled())
							{
								value = 0;
							}

							return value;
						};

					Sint16 ch1Value = ComputeChannelOutput(m_ch1Generator.GetOutput(), m_ch1LengthCounter, m_ch1VolumeEnvelope);
					Sint16 ch2Value = ComputeChannelOutput(m_ch2Generator.GetOutput(), m_ch2LengthCounter, m_ch2VolumeEnvelope);
					Sint16 ch3Value = m_ch3LengthCounter.IsChannelEnabled() ? m_ch3Generator.GetOutput() : 0;
					Sint16 ch4Value = ComputeChannelOutput(m_ch4Generator.GetOutput(), m_ch4LengthCounter, m_ch4VolumeEnvelope);

					Sint16 finalValue = ch1Value + ch2Value + ch3Value + ch4Value;

					//@TODO: mixing, etc.

					// Sine wave test
					//static float f = 0.0f;
					//f += m_sampleTimeStep;
					//ch2Value = sinf(f * 220.0f * 2 * 3.14f) * 4000.0f;

					*pCurrentSample++ = finalValue;
					*pCurrentSample++ = finalValue; 

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
			case Registers::NR10:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR10 | 0x80;
					}
					else
					{
						NR10 = value;
					}
					return true;
				}
				break;
			case Registers::NR11:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR11 | 0x3F;
					}
					else
					{
						NR11 = value;

						m_ch1LengthCounter.ResetLength();
					}
					return true;
				}
				break;
			case Registers::NR12:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR12 | 0x00;
					}
					else
					{
						NR12 = value;
					}
					return true;
				}
				break;
			case Registers::NR13:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR13 | 0xFF;
					}
					else
					{
						NR13 = value;
					}
					return true;
				}
				break;
			case Registers::NR14:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR14 | 0xBF;
					}
					else
					{
						NR14 = value;

						if (NR14 & Bit7)
						{
							// Channel is now enabled
							m_ch1Sweep.Reset();
							m_ch1Generator.Reset();
							m_ch1LengthCounter.Enable();
							m_ch1VolumeEnvelope.Reset();
							//@TODO: Wave channel's position is set to 0 but sample buffer is NOT refilled.
						}
					}
					return true;
				}
				break;

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
							//@TODO: Wave channel's position is set to 0 but sample buffer is NOT refilled.
						}
					}
					return true;
				}
				break;

			case Registers::NR30:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR30 | 0x7F;
					}
					else
					{
						NR30 = value;
					}
					return true;
				}
				break;
			case Registers::NR31:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR31 | 0xFF;
					}
					else
					{
						NR31 = value;

						m_ch3LengthCounter.ResetLength();
					}
					return true;
				}
				break;
			case Registers::NR32:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR32 | 0x9F;
					}
					else
					{
						NR32 = value;
					}
					return true;
				}
				break;
			case Registers::NR33:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR33 | 0xFF;
					}
					else
					{
						NR33 = value;
					}
					return true;
				}
				break;
			case Registers::NR34:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR34 | 0xBF;
					}
					else
					{
						NR34 = value;

						if (NR34 & Bit7)
						{
							// Channel is now enabled
							m_ch3Generator.Reset();
							m_ch3LengthCounter.Enable();
							//@TODO: Wave channel's position is set to 0 but sample buffer is NOT refilled.
						}
					}
					return true;
				}
				break;

			case Registers::NR41:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR41 | 0xFF;
					}
					else
					{
						NR41 = value;

						m_ch4LengthCounter.ResetLength();
					}
					return true;
				}
				break;
			case Registers::NR42:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR42 | 0x00;
					}
					else
					{
						NR42 = value;
					}
					return true;
				}
				break;
			case Registers::NR43:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR43 | 0x00;
					}
					else
					{
						NR43 = value;
					}
					return true;
				}
				break;
			case Registers::NR44:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = NR44 | 0xBF;
					}
					else
					{
						NR44 = value;

						if (NR44 & Bit7)
						{
							// Channel is now enabled
							m_ch4Generator.Reset();
							m_ch4LengthCounter.Enable();
							m_ch4VolumeEnvelope.Reset();
							//@TODO: Wave channel's position is set to 0 but sample buffer is NOT refilled.
						}
					}
					return true;
				}
				break;
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

	FrequencySweep m_ch1Sweep;
	SquareWaveGenerator m_ch1Generator;
	LengthCounter m_ch1LengthCounter;
	VolumeEnvelope m_ch1VolumeEnvelope;

	SquareWaveGenerator m_ch2Generator;
	LengthCounter m_ch2LengthCounter;
	VolumeEnvelope m_ch2VolumeEnvelope;
	
	WavetableGenerator m_ch3Generator;
	LengthCounter m_ch3LengthCounter;
	
	NoiseGenerator m_ch4Generator;
	LengthCounter m_ch4LengthCounter;
	VolumeEnvelope m_ch4VolumeEnvelope;
	
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