#include "Rom.h"
#include "Memory.h"
#include "Cpu.h"
#include "Utils.h"

#include "SDL.h"

#include <stdlib.h>
#include <stdio.h>
#include <memory>
#include <vector>

#include <Windows.h>

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
		GameBoy gb("cpu_instrs\\cpu_instrs.gb");
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

		bool done = false;

		Uint32 lastTicks = SDL_GetTicks();
		while (!done)
		{
			SDL_Event event;
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
		SDL_assert(false && "Uncaught exception thrown");
	}

    return 0;
}