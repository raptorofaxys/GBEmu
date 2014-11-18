#include "GameBoy.h"
#include "Utils.h"

#include "SDL.h"

#include <stdlib.h>
#include <stdio.h>
#include <memory>
#include <vector>

#include <Windows.h>

float g_totalCyclesExecuted = 0.0f;

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

		GameBoy gb("Tetris (JUE) (V1.1) [!].gb");
		//GameBoy gb("Super Mario Land (JUE) (V1.1) [!].gb");
		//GameBoy gb("Metroid II - Return of Samus (UE) [!].gb");
		//GameBoy gb("cpu_instrs\\cpu_instrs.gb");
		//GameBoy gb("cpu_instrs\\source\\test.gb");
		//GameBoy gb("cpu_instrs\\individual\\01-special.gb");
		//GameBoy gb("cpu_instrs\\individual\\02-interrupts.gb");
		//GameBoy gb("cpu_instrs\\individual\\03-op sp,hl.gb");
		//GameBoy gb("cpu_instrs\\individual\\04-op r,imm.gb");
		//GameBoy gb("cpu_instrs\\individual\\05-op rp.gb");
		//GameBoy gb("cpu_instrs\\individual\\06-ld r,r.gb");
		//GameBoy gb("cpu_instrs\\individual\\07-jr,jp,call,ret,rst.gb");
		//GameBoy gb("cpu_instrs\\individual\\08-misc instrs.gb"); // missing f2
		//GameBoy gb("cpu_instrs\\individual\\09-op r,r.gb");
		//GameBoy gb("cpu_instrs\\individual\\10-bit ops.gb"); // missing CB 40
		//GameBoy gb("cpu_instrs\\individual\\11-op a,(hl).gb"); // missing CB 46
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
			auto seconds = (ticks - lastTicks) / 1000.0f;
			//printf("%f\n", seconds);
			static float maxTimeStep = 0.1f;
			seconds = SDL_min(seconds, maxTimeStep);
			gb.Update(seconds);
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