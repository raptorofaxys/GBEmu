#include "Utils.h"

#include "SDL.h"

#include <stdlib.h>
#include <stdio.h>
#include <memory>
#include <vector>

#include <Windows.h>

void LoadFileAsByteArray(std::vector<Uint8>& output, const char* pFileName)
{
    SDL_RWops* pHandle = SDL_RWFromFile(pFileName, "rb");
    
	if (!pHandle)
	{
        throw Exception("Failed to load file %s.", pFileName);
    }

	SDL_RWseek(pHandle, 0,  RW_SEEK_END);
	int fileSize = static_cast<int>(SDL_RWtell(pHandle));
	SDL_RWseek(pHandle, 0,  RW_SEEK_SET);

	output.resize(fileSize);
	SDL_RWread(pHandle, output.data(), fileSize, 1);
    SDL_RWclose(pHandle);
}

std::shared_ptr<std::vector<Uint8>> LoadFileAsByteArray(const char* pFileName)
{
	std::shared_ptr<std::vector<Uint8>> pData(new std::vector<Uint8>);
	LoadFileAsByteArray(*pData, pFileName);

	return pData;
}

class Rom
{
public:
	Rom(const char* pFileName)
	{
		LoadFromFile(pFileName);
	}

	std::string GetRomName() const
	{
		std::string result;

		const Uint8* p = &m_pRom[kNameOffset];
		for (int i = 0; i < kNameLength; ++i)
		{
			if (*p)
			{
				result += *p;
			}
			++p;
		}

		return result;
	}

	const std::vector<Uint8>& GetRom()
	{
		return m_pRom;
	}

private:
	static const int kNameOffset = 0x134;
	static const int kNameLength = 0x11;

	void LoadFromFile(const char* pFileName)
	{
		LoadFileAsByteArray(m_pRom, pFileName);
	}

	std::vector<Uint8> m_pRom;
};

class Memory
{
public:
	static const int kRomBase = 0x0000;
	static const int kRomBaseBank00Size = 0x4000;
	
	static const int kWorkMemoryBase = 0xC000;
	static const int kWorkMemorySize = 0x2000; // 8k

	// Define memory addresses for all the memory-mapped registers
	enum class MemoryMappedRegisters
	{
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) name = addx,
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW
		//IF = 0xFF0F, // Interrupt Flag RW
		//IE = 0xFFFF, // Interrupt Enable RW
	};

	Memory(const std::shared_ptr<Rom>& rom)
		: m_pRom(rom)
	{
	}

	Uint8 Read8(Uint16 address)
	{
		if (IsAddressInRange(address, kRomBase, kRomBaseBank00Size))
		{
			return m_pRom->GetRom()[address];
		}
		else if (IsAddressInRange(address, kWorkMemoryBase, kWorkMemorySize))
		{
			return m_workMemory[address - kWorkMemoryBase];
		}
		else
		{
			switch (address)
			{
				// Handle reads to all the memory-mapped registers
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) case MemoryMappedRegisters::name: return m_##name;
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW
			default:
				throw NotImplementedException();
			}
		}
	}

	//Uint16 Read16(Uint16 address)
	//{
	//	// Must handle as two reads, because the address can cross range boundaries
	//	return Read8(address
	//}

	void Write8(Uint16 address, Uint8 value)
	{
		if (IsAddressInRange(address, kRomBase, kRomBaseBank00Size))
		{
			throw Exception("Attempted to write to ROM area.");
		}
		else if (IsAddressInRange(address, kWorkMemoryBase, kWorkMemorySize))
		{
			m_workMemory[address - kWorkMemoryBase] = value;
		}
		else
		{
			switch (address)
			{
				// Handle writes to all the memory-mapped registers
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) case MemoryMappedRegisters::name: m_##name = value; break;
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW
			default:
				throw NotImplementedException();
			}
		}
	}

private:

	bool IsAddressInRange(Uint16 address, Uint16 base, Uint16 rangeSize)
	{
		return (address >= base) && (address < base + rangeSize);
	}
	
	// Declare storage for all the memory-mapped registers
#define DEFINE_MEMORY_MAPPED_REGISTER_RW(addx, name) Uint8 m_##name;
#include "MemoryMappedRegisters.inc"
#undef DEFINE_MEMORY_MAPPED_REGISTER_RW

	std::shared_ptr<Rom> m_pRom;
	char m_workMemory[kWorkMemorySize];
};

class Cpu
{
public:
	static Uint32 const kCyclesPerSecond = 4194304;

	Cpu(const std::shared_ptr<Memory>& memory)
		: m_pMemory(memory)
	{
		Reset();
	}

	void Reset()
	{
		m_cyclesRemaining = 0.0f;

		IME = true;

		static_assert(offsetof(Cpu, F) == offsetof(Cpu, AF), "Target machine is not little-endian; register unions must be revised");
		PC = 0x0100;
		AF = 0x01B0;
		BC = 0x0013;
		DE = 0x00D8;
		HL = 0x014D;

		//(*m_pMemory)[0xFF05] = 0x00;
	}

	void Update(float seconds)
	{
		m_cyclesRemaining += seconds * kCyclesPerSecond;
		
		while (m_cyclesRemaining > 0)
		{
			m_cyclesConsumedByCurrentInstruction = 0;
			Uint8 opcode = Fetch8();

			Sint32 instructionCycles = -1;

			switch (opcode)
			{
				//@TODO: this will undoubtedly grow and be refactored.  Just want to have a few use cases to factor right.
			case 0x0: // NOP
				{
					instructionCycles = 4;
				}
				break;
			case 0x21: // LD HL,nn
				{
					instructionCycles = 12;
					HL = Fetch16();
				}
				break;
			case 0x31: // LD SP,nn
				{
					instructionCycles = 12;
					SP = Fetch16();
				}
				break;
			case 0x3E: // LD A,n
				{
					instructionCycles = 8;
					A = Fetch8();
				}
				break;
			case 0xC3: // JP nn
				{
					instructionCycles = 12;
					auto target = Fetch16();
					PC = target;
				}
				break;
			case 0xCD: // CALL nn
				{
					throw NotImplementedException();
				}
				break;
			case 0xE0: // LD (0xFF00+n),A
				{
					instructionCycles = 12;
					auto disp = Fetch8();
					auto address = disp + 0xFF00;
					Write8(address, A);
				}
				break;
			case 0xEA: // LD (nn),A
				{
					instructionCycles = 16;
					auto address = Fetch16();
					Write8(address, A);
				}
				break;
			case 0xF3: // DI
				{
					instructionCycles = 4;
					IME = false;
				}
				break;
			default:
				SDL_assert(false && "Unknown opcode encountered");
			}

			// Try to time things automatically, 
			SDL_assert(instructionCycles != -1);
			SDL_assert(m_cyclesConsumedByCurrentInstruction == instructionCycles);
		}
	}

private:
	void ConsumeCycles(Sint32 numCycles)
	{
		m_cyclesConsumedByCurrentInstruction += numCycles;
	}

	Uint8 Fetch8()
	{
		auto result = m_pMemory->Read8(PC);
		++PC;
		ConsumeCycles(4);
		return result;
	}

	Uint16 Fetch16()
	{
		auto low = Fetch8();
		auto high = Fetch8();
		return Make16(high, low);
	}

	void Write8(Uint16 address, Uint8 value)
	{
		m_pMemory->Write8(address, value);
		ConsumeCycles(4);
	}

	// This macro helps define register pairs that have alternate views.  For example, B and C can be indexed individually as 8-bit registers, but they can also be indexed together as a 16-bit register called BC. 
	// A static_assert helps make sure the machine endianness behaves as expected.
#define DualViewRegisterPair(High, Low) \
	union \
	{ \
		struct \
		{ \
			Uint8 Low; \
			Uint8 High; \
		}; \
		Uint16 High##Low; \
	};

    DualViewRegisterPair(A, F)
    DualViewRegisterPair(B, C)
    DualViewRegisterPair(D, E)
    DualViewRegisterPair(H, L)

#undef DualViewRegisterPair

	Uint16 SP;
	Uint16 PC;
	bool IME; // whether interrupts are enabled - very special register, not memory-mapped

	float m_cyclesRemaining;
	Sint32 m_cyclesConsumedByCurrentInstruction;

	std::shared_ptr<Memory> m_pMemory;
};

class GameBoy
{
public:
	static const int kScreenWidth = 160;
	static const int kScreenHeight = 144;

	GameBoy(const char* pFileName)
	{
		m_pRom.reset(new Rom(pFileName));
		m_pMemory.reset(new Memory(m_pRom));
		m_pCpu.reset(new Cpu(m_pMemory));
	}

	const Rom& GetRom() const
	{
		return *m_pRom;
	}

	void Update(float seconds)
	{
		m_pCpu->Update(seconds);
	}

private:
	std::shared_ptr<Rom> m_pRom;
	std::shared_ptr<Memory> m_pMemory;
	std::shared_ptr<Cpu> m_pCpu;
};

int main(int argc, char **argv)
{
	try
	{
		ProcessConsole console;

		SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			throw Exception("Couldn't initialize SDL: %s", SDL_GetError());
		}

		Janitor j([] { SDL_Quit(); });

		//GameBoy gb("Tetris (JUE) (V1.1) [!].gb");
		GameBoy gb("cpu_instrs.gb");
		//Rom rom("Tetris (JUE) (V1.1) [!].gb");
		const auto& gameName = gb.GetRom().GetRomName();

		std::shared_ptr<SDL_Window> pWindow(SDL_CreateWindow(gameName.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, GameBoy::kScreenWidth * 4, GameBoy::kScreenHeight * 4, 0), SDL_DestroyWindow);

		if (!pWindow)
		{
			throw Exception("Couldn't create window");
		}
		
		std::shared_ptr<SDL_Renderer> pRenderer(SDL_CreateRenderer(pWindow.get(), -1, 0), SDL_DestroyRenderer);
		if (!pRenderer)
		{
			throw Exception("Couldn't create renderer");
		}

		std::shared_ptr<SDL_Texture> pFrameBuffer(SDL_CreateTexture(pRenderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, GameBoy::kScreenWidth, GameBoy::kScreenHeight), SDL_DestroyTexture);
		if (!pRenderer)
		{
			throw Exception("Couldn't create framebuffer texture");
		}

		SDL_Event event;
		bool done = false;

		Uint32 lastTicks = SDL_GetTicks();
		while (!done)
		{
		    while (SDL_PollEvent(&event))
			{
		        switch (event.type)
				{
		        case SDL_KEYDOWN:
		            if (event.key.keysym.sym == SDLK_ESCAPE)
					{
		                done = true;
		            }
		            break;
		        case SDL_QUIT:
		            done = true;
		            break;
		        }
		    }

			Uint32 ticks = SDL_GetTicks();
			gb.Update((ticks - lastTicks) / 1000.0f);
			lastTicks = ticks;

			void* pPixels;
			int pitch;
			SDL_LockTexture(pFrameBuffer.get(), NULL, &pPixels, &pitch);
			Uint32* pARGB = static_cast<Uint32*>(pPixels);
			*pARGB = rand();
			SDL_UnlockTexture(pFrameBuffer.get());

		    SDL_RenderClear(pRenderer.get());
		    SDL_RenderCopy(pRenderer.get(), pFrameBuffer.get(), NULL, NULL);
		    SDL_RenderPresent(pRenderer.get());
		}
	}
	catch (const Exception& e)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Exception: %s", e.GetMessage());
	}

    return 0;
}