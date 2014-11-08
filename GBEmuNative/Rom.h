#pragma once

#include <string>
#include <vector>

#include "Utils.h"

#include "SDL.h"

class Rom
{
public:
	Rom(const char* pFileName)
	{
		LoadFromFile(pFileName);
	}

	std::string GetRomName() const
	{
		std::string result;

		const Uint8* p = &m_pRom[kNameOffset];
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

	const std::vector<Uint8>& GetRom()
	{
		return m_pRom;
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
