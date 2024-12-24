#pragma once

/* clang-format off */
#if defined(_WIN32) || defined(_WIN64)
	#define PLATFORM_WINDOWS

#elif defined(__linux__)
	#define PLATFORM_LINUX

#elif defined(__APPLE__)
	#include <TargetConditionals.h>
	#if defined(TARGET_OS_IPHONE) || defined(TARGET_IPHONE_SIMULATOR)
		#define PLATFORM_IOS

	#elif defined(TARGET_OS_MAC)
		#define PLATFORM_MACOSX

	#else
		#error Unknown Apple Platform

	#endif

#elif __ANDROID__
	#define PLATFORM_ANDROID

#else
	#error Unknown Platform

/* clang-format on */
#endif
