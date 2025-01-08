#ifndef CONFIG_H
#define CONFIG_H

#include "file_config.h"
#include "pcre2_config.h"

#ifdef _WIN32
typedef signed __int64 ssize_t;
typedef unsigned int mode_t;
#else
#include <unistd.h>
#endif

#endif // CONFIG_H
