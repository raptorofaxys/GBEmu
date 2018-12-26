#include "GameBoy.h"
#include "Utils.h"

#include "SDL.h"

#include <stdlib.h>
#include <stdio.h>
#include <memory>
#include <vector>
#include <chrono>

#include <Windows.h>
#include <direct.h>

float g_totalCyclesExecuted = 0.0f;

int main(int argc, char **argv)
{
	try
	{
		if (argc < 3)
		{
			throw Exception("Wrong syntax: %s <working directory> <rom>", argv[0]);
		}

		{
			auto workingDir = argv[1];
			auto result = _chdir(workingDir);
			if (result < 0)
			{
				throw Exception("Unable to switch to working directory");
			}
		}

		ProcessConsole console;

		SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0)
		{
			throw Exception("Couldn't initialize SDL: %s", SDL_GetError());
		}

		Janitor j([] { SDL_Quit(); });

		std::shared_ptr<SDL_Window> pWindow(SDL_CreateWindow("GBEmu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Lcd::kScreenWidth * 4, Lcd::kScreenHeight * 4, 0), SDL_DestroyWindow);
		if (!pWindow)
		{
			throw Exception("Couldn't create window");
		}
		
		std::shared_ptr<SDL_Renderer> pRenderer(SDL_CreateRenderer(pWindow.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC), SDL_DestroyRenderer);
		if (!pRenderer)
		{
			throw Exception("Couldn't create renderer");
		}

		GameBoy gb(argv[2], pRenderer.get());

		const auto& gameName = gb.GetRom().GetRomName();
		SDL_SetWindowTitle(pWindow.get(), gameName.c_str());

		bool done = false;

		auto lastMicroseconds = GetMicroseconds();
		
		float averageSeconds = -1.0f;
		auto lastPrintMicroseconds = static_cast<int64_t>(0);

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

			// Time- and FPS-tracking logic
			auto microseconds = GetMicroseconds();

			auto elapsedMicroseconds = microseconds - lastMicroseconds;
			auto seconds = elapsedMicroseconds / 1000000.0f;
			lastMicroseconds = microseconds;

			static float maxTimeStep = 0.1f;
			seconds = SDL_min(seconds, maxTimeStep);

			static float averagingRate = 0.3f;
			averageSeconds = (averageSeconds > 0.0f) ? (averageSeconds * (1.0f - averagingRate) + (seconds * averagingRate)) : seconds;
			if (microseconds - lastPrintMicroseconds > 200000)
			{
				SDL_SetWindowTitle(pWindow.get(), Format("%s - %3.1f FPS", gameName.c_str(), 1.0f / averageSeconds).c_str());
				//printf("%3.1f FPS\n", 1.0f / averageSeconds);
				lastPrintMicroseconds = microseconds;
			}

			gb.Update(seconds);

		    SDL_RenderClear(pRenderer.get());
		    SDL_RenderCopy(pRenderer.get(), gb.GetFrontFrameBufferTexture(), NULL, NULL);
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