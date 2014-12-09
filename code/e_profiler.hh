
#pragma once

#include "e_common.hh"

//#define F_ENABLE_PROFILING 1
#ifdef F_ENABLE_PROFILING

#include <chrono>
#include <map>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

extern std::map<std::string, float> g_profilerStatistics;

struct FNamedProfiler
{
    std::chrono::high_resolution_clock::time_point tmStart;
    const char* name;

    F_INLINE FNamedProfiler(const char* profName)
        : name(profName)
    {
        tmStart = std::chrono::high_resolution_clock::now();
    }

    F_INLINE ~FNamedProfiler()
    {
        auto tmEnd = std::chrono::high_resolution_clock::now();
        float msCount = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(tmEnd - tmStart).count();

        g_profilerStatistics[name] += msCount;
    }
};

#define F_NAMED_PROFILE(X) FNamedProfiler prof_##X(#X)
#else
#define F_NAMED_PROFILE(X)
#endif
