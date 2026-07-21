#ifndef TRACEEVENTITEM_H
#define TRACEEVENTITEM_H

#include <cstdint>
#include <string>

struct TraceEventItem {
	std::string name;
	std::string category;
	char phase = 0;
	uint64_t timestamp = 0;
	int64_t duration = 0; // only for complete events
	int32_t pid = 0;
	int32_t tid = 0;
	std::string args_comment;
};

#endif // TRACEEVENTITEM_H
