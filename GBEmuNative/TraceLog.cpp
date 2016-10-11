#include "TraceLog.h"

namespace TraceLog
{
	std::string s_traceLog;
	bool s_enabled;
	struct FlushOnDestroy
	{
		~FlushOnDestroy()
		{
			TraceLog::Flush();
		}
	};
	FlushOnDestroy s_flushOnDestroy;
}
