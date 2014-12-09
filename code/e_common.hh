
#pragma once

#include <stdint.h>

#ifdef _WIN32
#define F_INLINE __forceinline
#else
#define F_INLINE inline
#endif

#ifdef _MSC_VER
#pragma warning(disable:4996)
#define snprintf sprintf_s
#endif
