#pragma once

#include "Utils.h"

#include <string>
#include <stdio.h>

namespace
{
	static const char* TRACELOG_FILENAME = "tracelog.txt";
}

namespace TraceLog
{
	extern std::string s_traceLog;
	extern bool s_enabled;

	inline void SetEnabled(bool enabled)
	{
		s_enabled = enabled;
	}

	inline bool IsEnabled()
	{
		return s_enabled;
	}

	inline void Flush()
	{
		FILE* pFile;
		fopen_s(&pFile, TRACELOG_FILENAME, "a");
		if (pFile)
		{
			fwrite(s_traceLog.data(), s_traceLog.length(), 1, pFile);
			fclose(pFile);
		}
		s_traceLog.clear();
	}

	inline void Reset()
	{
		FILE* pFile;
		fopen_s(&pFile, TRACELOG_FILENAME, "wb");
		if (pFile)
		{
			fclose(pFile);
		}
		else
		{
			throw Exception("Can't reset log");
		}
	}

	inline void Log(const std::string& message)
	{
		if (!s_enabled)
		{
			return;
		}

		s_traceLog += message;

		static int const DUMP_BUFFER_SIZE = 1000000;
		if (s_traceLog.length() > DUMP_BUFFER_SIZE)
		{
			Flush();
		}
	}
}