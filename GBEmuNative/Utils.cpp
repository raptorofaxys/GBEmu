#include "Utils.h"

SDL_Keycode DebugWaitForKeypress()
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

SDL_Keycode DebugCheckForKeypress()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_KEYDOWN: return event.key.keysym.sym;
		}
	}
	return SDLK_UNKNOWN;
}

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
