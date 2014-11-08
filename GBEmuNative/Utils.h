#pragma once

#include <functional>
#include <vector>
#include <memory>

#include "SDL.h"
#include <Windows.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

inline Uint16 Make16(Uint8 high, Uint8 low) { return (high << 8) | low; }
inline Uint8 GetLow4(Uint8 u8) { return u8 & 0xF; }
inline Uint8 GetLow8(Uint16 u16) { return u16 & 0xFF; }
inline Uint8 GetHigh8(Uint16 u16) { return (u16 >> 8) & 0xFF; }

// WARING: the following functions eat up all events
SDL_Keycode DebugCheckForKeypress();
SDL_Keycode DebugWaitForKeypress();

void LoadFileAsByteArray(std::vector<Uint8>& output, const char* pFileName);
std::shared_ptr<std::vector<Uint8>> LoadFileAsByteArray(const char* pFileName);

class Exception
{
public:
	Exception(const char* pFormatter, ...)
	{
		SDL_assert(false && "Exception being constructed");

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

class NotImplementedException : public Exception
{
public:
	NotImplementedException() : Exception("The function is not implemented.") {}
};

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
