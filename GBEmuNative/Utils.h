#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <chrono>

#include "SDL.h"
#include <Windows.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

//@TODO: bit manipulation utils
enum
{
	Bit0 = 1 << 0,
	Bit1 = 1 << 1,
	Bit2 = 1 << 2,
	Bit3 = 1 << 3,
	Bit4 = 1 << 4,
	Bit5 = 1 << 5,
	Bit6 = 1 << 6,
	Bit7 = 1 << 7,
};

inline Uint8 GetLow4(Uint8 u8) { return u8 & 0xF; }
inline Uint8 GetHigh4(Uint8 u8) { return (u8 >> 4) & 0xF; }

inline Uint8 GetLow8(Uint16 u16) { return u16 & 0xFF; }
inline Uint16 GetLow12(Uint16 u16) { return u16 & 0xFFF; }
inline Uint8 GetHigh8(Uint16 u16) { return (u16 >> 8) & 0xFF; }

inline Uint16 Make16(Uint8 high, Uint8 low) { return (high << 8) | low; }

inline void SetBitValue(Uint8& byte, Uint8 position, bool value)
{
	auto bitMask = (1 << position);
	byte = value ? (byte | bitMask) : (byte & ~bitMask);
}

inline bool GetBitValue(Uint8 byte, Uint8 position)
{
	return (byte & (1 << position)) != 0;
}

inline bool IsAddressInRange(Uint16 address, Uint16 base, Uint16 rangeSize)
{
	return (address >= base) && (address < base + rangeSize);
}

inline int64_t GetMilliseconds()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::system_clock::now().time_since_epoch()).count();
}

void LoadFileAsByteArray(std::vector<Uint8>& output, const char* pFileName);
std::shared_ptr<std::vector<Uint8>> LoadFileAsByteArray(const char* pFileName);

inline std::string ReplaceAll(const std::string& str, const std::string& substring, const std::string& replacement)
{
	// Quite inefficient in terms of allocations
	auto result = str;
	auto substringLength = substring.length();
	for (;;)
	{
		auto pos = result.find(substring);
		if (pos == std::string::npos)
		{
			break;
		}

		result.replace(pos, substringLength, replacement);
	}

	return result;
}

inline std::string ReplaceFirst(const std::string& str, const std::string& substring, const std::string& replacement)
{
	// Quite inefficient in terms of allocations
	auto result = str;
	auto substringLength = substring.length();
	auto pos = result.find(substring);
	if (pos != std::string::npos)
	{
		result.replace(pos, substringLength, replacement);
	}

	return result;
}

inline std::string Format(const char* pFormatter, ...)
{
	va_list args;
	va_start(args, pFormatter);
	char szBuffer[4096];
	vsnprintf_s(szBuffer, ARRAY_SIZE(szBuffer), pFormatter, args);
	va_end(args);

	return std::string(szBuffer);
}

inline void DebugPrint(const char* pFormatter, ...)
{
	va_list args;
	va_start(args, pFormatter);
	char szBuffer[4096];
	vsnprintf_s(szBuffer, ARRAY_SIZE(szBuffer), pFormatter, args);
	va_end(args);

	OutputDebugStringA(szBuffer);
}

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

inline void SetForegroundConsoleColor()
{
	printf("\033[31;42m");
}

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
