#pragma once

#include "IMemoryBusDevice.h"

#include "Utils.h"

class Lcd : public IMemoryBusDevice
{
public:
	enum class Registers
	{
		LCDC = 0xFF40,	// LCD Control
		STAT = 0xFF41,	// LCDC Status
		SCY = 0xFF42,	// Scroll Y
		SCX = 0xFF43,	// Scroll X
		LY = 0xFF44,	// LCDC Y-coordinate
		LYC = 0xFF45,	// LY compare
		DMA = 0xFF46,	// DMA Transfer and start address
		BGP = 0xFF47,	// BG palette data
		OBP0 = 0xFF48,	// Object palette 0 data
		OBP1 = 0xFF49,	// Object palette 1 data
		WY = 0xFF4A,	// Window Y position
		WX = 0xFF4B,	// Window X position minus 7
	};

	enum class State
	{
		HBlank,
		//VBlank, // implicitly derived from the scanline
		ReadingOam,
		ReadingOamAndVram,
	};

	static const int kScreenWidth = 160;
	static const int kScreenHeight = 144;

	static const int kVramBase = 0x8000;
	static const int kVramSize = 0x2000;

	static const int kOamBase = 0xFE00;
	static const int kOamSize = 0xFE9F - kOamBase + 1;

	Lcd(const std::shared_ptr<MemoryBus>& memory, const std::shared_ptr<Cpu>& cpu, SDL_Renderer* pRenderer)
		: m_pMemory(memory)
		, m_pMemoryUnsafe(memory.get())
		, m_pCpu(cpu)
	{
		if (!pRenderer)
		{
			throw Exception("Couldn't create framebuffer texture");
		}
		auto createBufferFunc = [pRenderer]()
		{
			return std::shared_ptr<SDL_Texture>(SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, Lcd::kScreenWidth, Lcd::kScreenHeight), SDL_DestroyTexture);
		};
		m_pBackBuffer = createBufferFunc();
		m_pFrontBuffer = createBufferFunc();

		//@TODO SDL_QueryTexture
		//SDL_assert()
		Reset();
	}

	void Reset()
	{
		m_updateTimeLeft = 0.0f;
		m_nextState = State::ReadingOam;
		m_scanLine = 0;
		m_wasLcdEnabledLastUpdate = true;
		m_lastMode = 0;

		RenderDisabledFrameBuffer();

		memset(m_vram, 0xFD, sizeof(m_vram));
		memset(m_oam, 0xFD, sizeof(m_oam));

		LCDC = 0x91;
		STAT = 0;
		SCY = 0;
		SCX = 0;
		LY = 0;
		LYC = 0;
		DMA = 0;
		BGP = 0xFC;
		OBP0 = 0xFF;
		OBP1 = 0xFF;
		WY = 0;
		WX = 0;
	}

	SDL_Texture* GetFrontFrameBufferTexture() const
	{
		return m_pFrontBuffer.get();
	}

	void Update(float seconds)
	{
		const float timeStep = 1.0f / MemoryBus::kCyclesPerSecond;

		// Documentation on the exact timing here quotes various numbers.
		m_updateTimeLeft += seconds;

		while (m_updateTimeLeft > 0.0f)
		{
			int mode = 0;
			bool isLcdEnabled = (LCDC & Bit7) != 0;
			if (isLcdEnabled)
			{
				switch (m_nextState)
				{
				case State::ReadingOam:
					{
						++m_scanLine;
						++LY;

						if (m_scanLine > 153)
						{
							m_scanLine = 0;
							LY = 0;
						}

						if (LY == LYC)
						{
							STAT |= Bit2;

							if (STAT & Bit6)
							{
								m_pCpu->SignalInterrupt(Bit1);
							}
						}
						else
						{
							STAT &= ~Bit2;
						}

						RenderScanline();

						//m_updateTimeLeft -= 0.000019f;
						m_updateTimeLeft -= 80 * timeStep;
						mode = 2;
						m_nextState = State::ReadingOamAndVram;
					}
					break;
				case State::ReadingOamAndVram:
					{
						//m_updateTimeLeft -= 0.000041f;
						m_updateTimeLeft -= 172 * timeStep;
						mode = 3;
						m_nextState = State::HBlank;
					}
					break;
				case State::HBlank:
					{
						//m_updateTimeLeft -= 0.0000486f;
						m_updateTimeLeft -= 204 * timeStep;
						mode = 0;
						m_nextState = State::ReadingOam;
					}
					break;
				}

				if (m_scanLine >= 144)
				{
					// Vblank
					mode = 1;
				}

				if (mode != m_lastMode)
				{
					switch (mode)
					{
					case 0:
						// HBlank interrupt
						if (STAT & Bit3)
						{
							m_pCpu->SignalInterrupt(Bit1);
						}
						break;
					case 1:
						// VBlank interrupt
						if (STAT & Bit4)
						{
							m_pCpu->SignalInterrupt(Bit1);
						}
						// Always fire the blank into IF
						m_pCpu->SignalInterrupt(Bit0);
						SwapFrameBuffers();
						break;
					case 2:
						// Reading OAM interrupt
						if (STAT & Bit5)
						{
							m_pCpu->SignalInterrupt(Bit1);
						}
						break;
					}
				}

				m_lastMode = mode;
			}
			else
			{
				// LCD is disabled
				mode = 1;
				m_lastMode = 1;
				m_updateTimeLeft = 0.0f;
				m_scanLine = -1;
				LY = 0;
				m_nextState = State::ReadingOam;
			}

			if (m_wasLcdEnabledLastUpdate && !isLcdEnabled)
			{
				RenderDisabledFrameBuffer();
			}
			m_wasLcdEnabledLastUpdate = isLcdEnabled;

			// Mode is the lower two bits of the STAT register
			STAT = (STAT & ~(Bit1 | Bit0)) | (mode);
		}
	}

	void RenderDisabledFrameBuffer()
	{
		void* pVoidPixels;
		int pitch;
		SDL_LockTexture(m_pBackBuffer.get(), NULL, &pVoidPixels, &pitch);

		char* pPixels = reinterpret_cast<char*>(pVoidPixels);

		//@TODO: replace with a memset or something, but in the meantime this allows for patterns to help debugging
		for (Sint16 x = 0; x < kScreenWidth; ++x)
		{
			for (Sint16 y = 0; y < kScreenHeight; ++y)
			{
				Uint32* pARGB = reinterpret_cast<Uint32*>(pPixels + y * pitch + x * 4);
				
				Uint8 r = 0xFF;
				Uint8 g = 0xFF; //0x00;
				Uint8 b = 0xFF;
				*pARGB = 0xFF000000 | (r << 16) | (g << 8) | b;
			}
		}
		
		SDL_UnlockTexture(m_pBackBuffer.get());
		SwapFrameBuffers();
	}

	Uint8 ReadVram(Uint16 address)
	{
		Uint16 offset = address - kVramBase;
		SDL_assert(offset < kVramSize);

		return m_vram[offset];
	}

	Uint8 ReadOam(Uint16 address)
	{
		Uint16 offset = address - kOamBase;
		SDL_assert(offset < kOamSize);

		return m_oam[offset];
	}

	Uint8 GetTileIndexAtXY(Uint16 tileMapBaseAddress, int x, int y)
	{
		// Tiles are 8x8; see which tile we're in
		Uint8 tileMapX = x / 8;
		Uint8 tileMapY = y / 8;

		// Tile maps are 32x32
		Uint16 tileOffset = tileMapY * 32 + tileMapX;

		Uint8 tileIndex = ReadVram(tileMapBaseAddress + tileOffset);
		
		return tileIndex;
	}

	Uint8 GetTileDataPixelColorIndex(Uint16 baseTileDataAddress, Sint16 tileIndex, int x, int y)
	{
		// Fetch the pixel's color index from the tile data
		Uint8 tileDataX = x % 8;
		Uint8 tileDataY = y % 8;
		Uint8 tileDataShift = 7 - tileDataX;
		Uint8 tileDataMask = 1 << tileDataShift;

		// Each tile's data occupies 16 bytes, and each row of tile data occupies two bytes
		Uint16 tileDataAddress = baseTileDataAddress + tileIndex * 16 + tileDataY * 2;

		Uint8 tileRowLsb = (ReadVram(tileDataAddress) & tileDataMask) >> tileDataShift;
		Uint8 tileRowMsb = (ReadVram(tileDataAddress + 1) & tileDataMask) >> tileDataShift;

		Uint8 colorIndex = (tileRowMsb << 1) | tileRowLsb;

		return colorIndex;
	}

	Uint8 GetLuminosityForColorIndex(Uint8 paletteRegister, Uint8 colorIndex)
	{
		// Translate the color index to an actual color using the palette registers
		Uint8 shadeShift = 2 * colorIndex;
		Uint8 shadeMask = 0x3 << shadeShift;
		Uint8 shade = (paletteRegister & shadeMask) >> shadeShift;

		Uint8 luminosity = (3 - shade) * 0x55;
		
		return luminosity;
	}

	void RenderScanline()
	{
		if (LY < kScreenHeight)
		{
			void* pPixels;
			int pitch;
			SDL_LockTexture(m_pBackBuffer.get(), NULL, &pPixels, &pitch);

			Uint32* pARGB = reinterpret_cast<Uint32*>(static_cast<Uint8*>(pPixels) + LY * pitch);

			//static Uint8 palette[] = {0xFF, 0xC0, }

			static int c = 0;
			for (int screenX = 0; screenX < kScreenWidth; ++screenX)
			{
				Uint8 a = 0xFF;
				Uint8 r = 0xFF;
				Uint8 g = 0xFF;
				Uint8 b = 0xFF;
				Uint8 luminosity = 0;

				bool backgroundIsTransparent = false;

				if (LCDC & Bit0)
				{
					// Background is active
					Uint16 x = (SCX + screenX) % 256;
					Uint16 y = (SCY + m_scanLine) % 256;

					Uint16 tileMapBaseAddress = (LCDC & Bit3) ? 0x9C00 : 0x9800;
					Sint16 tileIndex = GetTileIndexAtXY(tileMapBaseAddress, x, y);

					// Find the tile data
					Uint16 baseTileDataAddress = 0;
					if (LCDC & Bit4)
					{
						baseTileDataAddress = 0x8000;
					}
					else
					{
						baseTileDataAddress = 0x9000; // tile data starts at 0x8800, but it's indexed using signed values so tile 0 is at 0x9000
						if (tileIndex > 127)
						{
							tileIndex -= 256;
						}
					}

					auto colorIndex = GetTileDataPixelColorIndex(baseTileDataAddress, tileIndex, x, y);

					backgroundIsTransparent = (colorIndex == 0);
					
					luminosity = GetLuminosityForColorIndex(BGP, colorIndex);
				}

				static bool enableWindow = true;
				if (enableWindow && (LCDC & Bit5))
				{
					// Window is active - always displayed above background
					Sint16 x = screenX - (WX - 7);
					Sint16 y = m_scanLine - WY;

					if ((x >= 0) && (x < 160) && (y >= 0) && (y < 144))
					{
						Uint16 tileMapBaseAddress = (LCDC & Bit6) ? 0x9C00 : 0x9800;
						Sint8 tileIndex = GetTileIndexAtXY(tileMapBaseAddress, x, y); // Window tiles are always signed

						// Find the tile data
						Uint16 baseTileDataAddress = 0x9000;
						auto colorIndex = GetTileDataPixelColorIndex(baseTileDataAddress, tileIndex, x, y);

						backgroundIsTransparent = (colorIndex == 0);

						luminosity = GetLuminosityForColorIndex(BGP, colorIndex);
					}
				}

				if (LCDC & Bit1)
				{
					// Sprites are active

					bool sprites8x16 = ((LCDC & Bit2) != 0);

					Sint16 bestBaseX;
					int bestIndex = -1;
					Uint8 bestLuminosity;
					Uint8 bestAttributes;

					// Find the best sprite hit for this pixel
					for (int spriteIndex = 0; spriteIndex < 40; ++spriteIndex)
					{
						Uint16 spriteBaseAddress = 0xFE00 + spriteIndex * 4;
						Sint16 spriteBaseX = ReadOam(spriteBaseAddress + 1) - 8;
						Sint16 spriteBaseY = ReadOam(spriteBaseAddress + 0) - 16;

						Sint16 x = screenX - spriteBaseX;
						Sint16 y = m_scanLine - spriteBaseY;

						Uint8 tileIndex = ReadOam(spriteBaseAddress + 2);
						Uint8 attributes = ReadOam(spriteBaseAddress + 3);

						bool verticalFlip = ((attributes & Bit6) != 0);

						if (sprites8x16)
						{
							if (y >= 8)
							{
								y -= 8;
								
								if (!verticalFlip)
								{
									tileIndex |= 1;
								}
								else
								{
									tileIndex &= ~1;
								}
							}
							else
							{
								if (!verticalFlip)
								{
									tileIndex &= ~1;
								}
								else
								{
									tileIndex |= 1;
								}
							}
						}

						// Horizontal flip
						if (attributes & Bit5)
						{
							x = 7 - x;
						}

						// Vertical flip
						if (verticalFlip)
						{
							y = 7 - y;
						}

						auto colorIndex = GetTileDataPixelColorIndex(0x8000, tileIndex, x, y);
						
						Uint8 palette = ((attributes & Bit4) != 0) ? OBP1 : OBP0;
						auto spriteLuminosity = GetLuminosityForColorIndex(palette, colorIndex);

						if ((colorIndex != 0) && (x >= 0) && (x < 8) && (y >= 0) && (y < 8))
						{
							if ((bestIndex < 0) || (spriteBaseX < bestBaseX))
							{
								bestBaseX = spriteBaseX;
								bestIndex = spriteIndex;
								bestLuminosity = spriteLuminosity;
								bestAttributes = attributes;
							}
						}
					}

					if (bestIndex >= 0)
					{
						if (bestAttributes & Bit7)
						{
							// Sprite is behind background, it only shows if the background is transparent
							if (backgroundIsTransparent)
							{
								luminosity = bestLuminosity;
							}
						}
						else
						{
							// Sprite is in front of background, it always shows
							luminosity = bestLuminosity;
						}
					}
				}

				r = luminosity;
				g = luminosity;
				b = luminosity;

				*pARGB = 0xFF000000 | (r << 16) | (g << 8) | b;

				++pARGB;
			}
			++c;

			if (LY == 0)
			{
				++c;
			}

			SDL_UnlockTexture(m_pBackBuffer.get());
		}
	}

	void SwapFrameBuffers()
	{
		std::swap(m_pFrontBuffer, m_pBackBuffer);
	}

	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value)
	{
		if (ServiceMemoryRangeRequest(requestType, address, value, kVramBase, kVramSize, m_vram))
		{
			return true;
		}
		else if (ServiceMemoryRangeRequest(requestType, address, value, kOamBase, kOamSize, m_oam))
		{
			return true;
		}
		else
		{
			switch (address)
			{
			SERVICE_MMR_RW(LCDC)

			case Registers::STAT:
				{
					if (requestType == MemoryRequestType::Read)
					{
						value = STAT;
					}
					else
					{
						// Bits 3-6 are read/write, bits 0-2 are read-only
						STAT = (value & (Bit6 | Bit5 | Bit4 | Bit3)) | (STAT & (Bit2 | Bit1 | Bit0));
					}
					return true;
				}
		
			SERVICE_MMR_RW(SCY)
			SERVICE_MMR_RW(SCX)

			case Registers::LY:
				{
					if (requestType == MemoryRequestType::Write)
					{
						LY = 0;
					}
					else
					{
						value = LY;
					}
					return true;
				}

			SERVICE_MMR_RW(LYC)

			case Registers::DMA:
				{
					if (requestType == MemoryRequestType::Write)
					{
						auto dmaSourceAddress = value << 8;
						auto dmaDestinationAddress = 0xFE00;

						for ( ; dmaDestinationAddress <= 0xFE9F; ++dmaSourceAddress, ++dmaDestinationAddress)
						{
							m_pMemoryUnsafe->Write8(dmaDestinationAddress, m_pMemoryUnsafe->Read8(dmaSourceAddress));
						}
						//@TODO: DMA transfer time emulation
					}
					else
					{
						value = 0;
					}
					return true;
				}
				break;

			SERVICE_MMR_RW(BGP)
			SERVICE_MMR_RW(OBP0)
			SERVICE_MMR_RW(OBP1)
			SERVICE_MMR_RW(WY)
			SERVICE_MMR_RW(WX)
			}
		}
	
		return false;
	}
private:
	float m_updateTimeLeft;
	State m_nextState;
	int m_scanLine;
	bool m_wasLcdEnabledLastUpdate;
	int m_lastMode;

	Uint8 m_vram[kVramSize];
	Uint8 m_oam[kOamSize];

	Uint8 LCDC;
	Uint8 STAT;
	Uint8 SCY;
	Uint8 SCX;
	Uint8 LY;
	Uint8 LYC;
	Uint8 DMA;
	Uint8 BGP;
	Uint8 OBP0;
	Uint8 OBP1;
	Uint8 WY;
	Uint8 WX;

	std::shared_ptr<MemoryBus> m_pMemory;
	MemoryBus* m_pMemoryUnsafe;
	std::shared_ptr<Cpu> m_pCpu;
	std::shared_ptr<SDL_Texture> m_pBackBuffer;
	std::shared_ptr<SDL_Texture> m_pFrontBuffer;
};