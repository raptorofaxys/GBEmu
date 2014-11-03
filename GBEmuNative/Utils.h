#pragma once

#include <functional>

#include "SDL.h"
#include <Windows.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

//template <typename T>
//bool IsBetween(T value, T min, T max) { return (value >= min) && (value <= max); }
//bool IsBetween(Uint16 value, Uint16 min, Uint16 max) { return IsBetween<Uint16>(value, min, max); }

Uint16 Make16(Uint8 high, Uint8 low) { return (high << 8) | low; }

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
