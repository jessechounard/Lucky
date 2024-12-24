#pragma once

#include <Lucky/Utility/Platform.h>

/* clang-format off */

#if defined(PLATFORM_WINDOWS)
	#include <glad/gl.h>
    #define GL_DESKTOP

#elif defined(PLATFORM_MACOSX)
	#include <OpenGL/gl.h>
	#define GL_DESKTOP

#elif defined(PLATFORM_LINUX)
	#error Linux not yet supported
	// todo: implement
	#define GL_DESKTOP

#elif defined(PLATFORM_IOS)
	#include <OpenGLES/ES2/gl.h>
	#include <OpenGLES/ES2/glext.h>
	#define GL_ES2

#elif defined(PLATFORM_ANDROID)
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
    #define GL_ES2

/* clang-format on */

#endif
