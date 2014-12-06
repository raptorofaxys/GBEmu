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
		WX = 0xFF4B,	// WIndow X position minus 7
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

	Lcd(const std::shared_ptr<MemoryBus>& memory, const std::shared_ptr<Cpu>& cpu, const std::shared_ptr<SDL_Texture>& pFrameBuffer)
		: m_pMemory(memory)
		, m_pMemoryUnsafe(memory.get())
		, m_pCpu(cpu)
		, m_pFrameBuffer(pFrameBuffer)
	{
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

	void Update(float seconds)
	{
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

						if (LY == LYC)
						{
							if (STAT & Bit6)
							{
								m_pCpu->SignalInterrupt(Bit1);
							}

							STAT |= Bit2;
						}
						else
						{
							STAT &= ~Bit2;
						}

						if (STAT & Bit5)
						{
							m_pCpu->SignalInterrupt(Bit1);
						}

						if ((STAT & Bit4) && (m_scanLine == 144))
						{
							m_pCpu->SignalInterrupt(Bit0);
						}

						if (m_scanLine > 153)
						{
							m_scanLine = 0;
							LY = 0;
						}

						RenderScanline();

						m_updateTimeLeft -= 0.000019f;
						mode = 2;
						m_nextState = State::ReadingOamAndVram;
					}
					break;
				case State::ReadingOamAndVram:
					{
						m_updateTimeLeft -= 0.000041f;
						mode = 3;
						m_nextState = State::HBlank;
					}
					break;
				case State::HBlank:
					{
						if (STAT & Bit3)
						{
							m_pCpu->SignalInterrupt(Bit1);
						}

						m_updateTimeLeft -= 0.0000486f;
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
			}
			else
			{
				// LCD is disabled
				mode = 1;
				m_updateTimeLeft = 0.0f;
				m_scanLine = -1;
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
		SDL_LockTexture(m_pFrameBuffer.get(), NULL, &pVoidPixels, &pitch);

		char* pPixels = reinterpret_cast<char*>(pVoidPixels);

		//@TODO: replace with a memset or something, but in the meantime this allows for patterns to help debugging
		for (Sint16 x = 0; x < kScreenWidth; ++x)
		{
			for (Sint16 y = 0; y < kScreenHeight; ++y)
			{
				Uint32* pARGB = reinterpret_cast<Uint32*>(pPixels + y * pitch + x * 4);
				
				Uint8 r = 0xFF;
				Uint8 g = 0x00;
				Uint8 b = 0xFF;
				*pARGB = 0xFF000000 | (r << 16) | (g << 8) | b;
			}
		}
		
		SDL_UnlockTexture(m_pFrameBuffer.get());
	}

	void RenderScanline()
	{
		if (LY < kScreenHeight)
		{
			//@TODO: Bit 6 - Window Tile Map Display Select(0 = 9800 - 9BFF, 1 = 9C00 - 9FFF)
			//@TODO: Bit 5 - Window Display Enable(0 = Off, 1 = On)
			//@TODO: Bit 2 - OBJ(Sprite) Size(0 = 8x8, 1 = 8x16)
			//@TODO: Bit 1 - OBJ(Sprite) Display Enable(0 = Off, 1 = On)
			void* pPixels;
			int pitch;
			SDL_LockTexture(m_pFrameBuffer.get(), NULL, &pPixels, &pitch);

			Uint32* pARGB = reinterpret_cast<Uint32*>(static_cast<Uint8*>(pPixels) + LY * pitch);

			//static Uint8 palette[] = {0xFF, 0xC0, }

			static int c = 0;
			Uint16 y = (SCY + m_scanLine) % 256;
			for (int screenX = 0; screenX < kScreenWidth; ++screenX)
			{
				Uint8 a = 0xFF;
				Uint8 r = 0xFF;
				Uint8 g = 0xFF;
				Uint8 b = 0xFF;

				if (LCDC & Bit0)
				{
					// Background is active
					Uint16 x = (SCX + screenX) % 256;

					// Tiles are 8x8; see which tile we're in
					Uint8 tileMapX = x / 8;
					Uint8 tileMapY = y / 8;

					// Background tile map is 32x32
					Uint16 tileOffset = tileMapY * 32 + tileMapX;
					Uint16 tileMapBaseAddress = (LCDC & Bit3) ? 0x9C00 : 0x9800;

					Sint16 tileIndex = m_pMemoryUnsafe->Read8(tileMapBaseAddress + tileOffset);

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

					// Fetch the pixel's color index from the tile data
					Uint8 tileDataX = x % 8;
					Uint8 tileDataY = y % 8;
					Uint8 tileDataShift = 7 - tileDataX;
					Uint8 tileDataMask = 1 << tileDataShift;

					// Each tile's data occupies 16 bytes, and each row of tile data occupies two bytes
					Uint16 tileDataAddress = baseTileDataAddress + tileIndex * 16 + tileDataY * 2;

					Uint8 tileRowLsb = (m_pMemoryUnsafe->Read8(tileDataAddress) & tileDataMask) >> tileDataShift;
					Uint8 tileRowMsb = (m_pMemoryUnsafe->Read8(tileDataAddress + 1) & tileDataMask) >> tileDataShift;

					Uint8 colorIndex = (tileRowMsb << 1) | tileRowLsb;
					//colorIndex = rand() % 4;

					// Translate the color index to an actual color using the palette registers
					Uint8 shadeShift = 2 * colorIndex;
					Uint8 shadeMask = 0x3 << shadeShift;
					Uint8 shade = (BGP & shadeMask) >> shadeShift;

					Uint8 luminosity = (3 - shade) * 0x55;

					r = luminosity;
					g = luminosity;
					b = luminosity;
				}

				//*pARGB = 0xFF000000 | (0xFF << ((c % 3) * 8));
				//if (!(LCDC & Bit7))
				//{
				//	// Display is off
				//	r = 0xFF;
				//	g = 0;
				//	//g = 0xFF;
				//	b = 0xFF;
				//}

				*pARGB = 0xFF000000 | (r << 16) | (g << 8) | b;

				++pARGB;
			}
			++c;

			if (LY == 0)
			{
				++c;
			}

			SDL_UnlockTexture(m_pFrameBuffer.get());
		}
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
							m_pMemory->Write8(dmaDestinationAddress, m_pMemory->Read8(dmaSourceAddress));
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
	std::shared_ptr<SDL_Texture> m_pFrameBuffer;
};