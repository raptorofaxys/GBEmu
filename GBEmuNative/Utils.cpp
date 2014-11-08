#include "Utils.h"

SDL_Keycode WaitForKeypress()
{
	SDL_Event event;
	for (;;)
	{
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_KEYDOWN: return event.key.keysym.sym;
			}
		}
		SDL_Delay(10);
	}
	return SDLK_UNKNOWN;
}