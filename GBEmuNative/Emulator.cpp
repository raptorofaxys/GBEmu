#include "Utils.h"

#include "SDL.h"

#include <stdlib.h>
#include <stdio.h>
#include <memory>
#include <vector>
#include <functional>

#include <Windows.h>

class Exception
{
public:
	Exception(const char* pFormatter, ...)
	{
		va_list args;
		va_start(args, pFormatter);
		char szBuffer[4096];
		vsnprintf_s(szBuffer, ARRAY_SIZE(szBuffer), pFormatter, args);
		va_end(args);

		m_error = szBuffer;
	}

	const char* GetMessage() const
	{
		return m_error.c_str();
	}
		
private:
	std::string m_error;
};

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

	std::string GetRomName()
	{
		std::string result;

		Uint8* p = &m_pRom[kNameOffset];
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

private:
	static const int kNameOffset = 0x134;
	static const int kNameLength = 0x11;

	void LoadFromFile(const char* pFileName)
	{
		LoadFileAsByteArray(m_pRom, pFileName);
	}

	std::vector<Uint8> m_pRom;
};

class GameBoy
{
public:
	static int const kScreenWidth = 160;
	static int const kScreenHeight = 144;
};

//template<typename L>
//struct Janitor
//{
//
//};
//
//template<typename L> Janitor<L> MakeJanitor(L& lambda)
//{
//	return Janitor<L>(lambda);
//}

struct Janitor
{
	typedef std::function<void()> FuncType;
	Janitor(FuncType f)
		: m_func(f)
		, m_released(false)
	{
	}

	~Janitor()
	{
		if (!m_released)
		{
			Invoke();
		}
	}

	void Release()
	{
		m_released = true;
	}

	void Invoke()
	{
		m_func();
		Release();
	}

	bool m_released;
	FuncType m_func;
};

class ProcessConsole
{
public:
	ProcessConsole()
	{
		AllocConsole();
		FILE* pFile;
		freopen_s(&pFile, "CON", "w", stdout);
	}

	~ProcessConsole()
	{
		FreeConsole();
	}
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

		Rom rom("Tetris (JUE) (V1.1) [!].gb");
		const auto& gameName = rom.GetRomName();

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