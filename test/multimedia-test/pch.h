//
// pch.h
// Header for standard system include files.
//

#pragma once
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX

#define _WIN32_WINNT 0x0A00 // Windows 10  
#define _ENABLE_EXTENDED_ALIGNED_STORAGE
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#include "gtest/gtest.h"
#pragma comment(lib,"Ws2_32")