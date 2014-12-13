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
		//@TODO: change targetname per configuration
		ProcessConsole console;

		SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			throw Exception("Couldn't initialize SDL: %s", SDL_GetError());
		}

		Janitor j([] { SDL_Quit(); });

		std::shared_ptr<SDL_Window> pWindow(SDL_CreateWindow("GBEmu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Lcd::kScreenWidth * 4, Lcd::kScreenHeight * 4, 0), SDL_DestroyWindow);

		if (!pWindow)
		{
			throw Exception("Couldn't create window");
		}
		
		std::shared_ptr<SDL_Renderer> pRenderer(SDL_CreateRenderer(pWindow.get(), -1, 0), SDL_DestroyRenderer);
		if (!pRenderer)
		{
			throw Exception("Couldn't create renderer");
		}

		//GameBoy gb("cpu_instrs\\cpu_instrs.gb", pRenderer.get());
		//GameBoy gb("cpu_instrs\\source\\test.gb");
		//GameBoy gb("cpu_instrs\\individual\\01-special.gb");
		//GameBoy gb("cpu_instrs\\individual\\02-interrupts.gb");
		//GameBoy gb("cpu_instrs\\individual\\03-op sp,hl.gb");
		//GameBoy gb("cpu_instrs\\individual\\04-op r,imm.gb");
		//GameBoy gb("cpu_instrs\\individual\\05-op rp.gb");
		//GameBoy gb("cpu_instrs\\individual\\06-ld r,r.gb");
		//GameBoy gb("cpu_instrs\\individual\\07-jr,jp,call,ret,rst.gb");
		//GameBoy gb("cpu_instrs\\individual\\08-misc instrs.gb");
		//GameBoy gb("cpu_instrs\\individual\\09-op r,r.gb");
		//GameBoy gb("cpu_instrs\\individual\\10-bit ops.gb");
		//GameBoy gb("cpu_instrs\\individual\\11-op a,(hl).gb");

		//GameBoy gb("Alleyway (JUE) [!].gb", pRenderer.get()); // missing external RAM
		//GameBoy gb("Balloon Kid (JUE) [!].gb", pRenderer.get()); // renders white
		//GameBoy gb("F-1 Race (JUE) (V1.1) [!].gb", pRenderer.get()); // MBC2 + battery
		//GameBoy gb("Metroid II - Return of Samus (UE) [!].gb");
		//GameBoy gb("SolarStriker (JU) [!].gb", pRenderer.get());
		GameBoy gb("Super Mario Land (JUE) (V1.1) [!].gb", pRenderer.get());
		//GameBoy gb("Tetris (JUE) (V1.1) [!].gb", pRenderer.get());

		const auto& gameName = gb.GetRom().GetRomName();
		SDL_SetWindowTitle(pWindow.get(), gameName.c_str());

		bool done = false;

		Uint32 lastTicks = SDL_GetTicks();
		
		float averageSeconds = -1.0f;
		Uint32 lastPrintTicks = 0;

		while (!done)
		{
			SDL_Event event;
		    while (SDL_PollEvent(&event))
			{
		        switch (event.type)
				{
				case SDL_KEYDOWN:
					{
						switch (event.key.keysym.sym)
						{
						case SDLK_ESCAPE:
							done = true;
							break;
						case SDLK_s:
							gb.Step();
							break;
						case SDLK_g:
							gb.Go();
							break;
						case SDLK_d:
							gb.BreakInDebugger();
							break;
						case SDLK_n:
							gb.BreakAtNextInstruction();
							break;
						}
					}
					break;
		        case SDL_QUIT:
		            done = true;
		            break;
		        }
		    }

			Uint32 ticks = SDL_GetTicks();

			auto seconds = (ticks - lastTicks) / 1000.0f;
			static float maxTimeStep = 0.1f;
			seconds = SDL_min(seconds, maxTimeStep);

			static float averagingRate = 0.5f;
			averageSeconds = (averageSeconds > 0.0f) ? (averageSeconds * (1.0f - averagingRate) + (seconds * averagingRate)) : seconds;
			if (ticks - lastPrintTicks > 1000)
			{
				SDL_SetWindowTitle(pWindow.get(), Format("%s - %3.1f FPS", gameName.c_str(), 1.0f / averageSeconds).c_str());
				//printf("%3.1f FPS\n", 1.0f / averageSeconds);
				lastPrintTicks = ticks;
			}

			gb.Update(seconds);
			lastTicks = ticks;

		    SDL_RenderClear(pRenderer.get());
		    SDL_RenderCopy(pRenderer.get(), gb.GetFrameBufferTexture(), NULL, NULL);
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