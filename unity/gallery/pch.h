#pragma once

#if defined UNITYAPI || defined DLLAPI
#error "macro naming collision"
#pragma push_marcro("UNITYAPI")
#pragma push_marcro("DLLAPI")
#endif

#ifndef STATIC_LIBRARY
#ifdef GALLERY_EXPORTS
#define DLLAPI __declspec(dllexport)
#define UNITYAPI  __declspec(dllexport) __stdcall
#else   // GALLERY_EXPORTS
#define DLLAPI __declspec(dllimport)
#define UNITYAPI  __declspec(dllimport) __stdcall
#endif  // GALLERY_EXPORTS
#else   // STATIC_LIBRARY
#define DLLAPI 
#define UNITYAPI 
#endif  // STATIC_LIBRARY

#include "unity/detail/IUnityGraphics.h"
#include "unity/gallery/export.h"
#include <boost/circular_buffer.hpp>
