#pragma once

#ifdef _WIN32
#include <cstdint>
using Fixed = std::int32_t;
using Boolean = unsigned char;
using Handle = void**;
struct FSRef;
using CFBundleRef = const struct __CFBundle*;
#endif
