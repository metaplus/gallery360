#pragma once

#if defined DLL_EXPORT
#error "macro DLL_EXPORT naming collision"
#endif

#ifndef DLL_STATIC_LINK
#ifdef GALLERY_EXPORTS
#define DLL_EXPORT __declspec(dllexport)
#else   // GALLERY_EXPORTS
#define DLL_EXPORT __declspec(dllimport)
#endif  // GALLERY_EXPORTS
#else
#define DLLAPI 
#endif

#pragma warning(push)
#pragma warning(disable: 4819)
#include "unity/IUnityGraphics.h"
#include "unity/IUnityRenderingExtensions.h"
#pragma warning(pop)
#include "gallery/plugin.export.h"
