//
// pch.h
// Header for standard system include files.
//

#pragma once

#define _ENABLE_EXTENDED_ALIGNED_STORAGE
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#include "gtest/gtest.h"
#pragma comment(lib, "Ws2_32")
#pragma comment(lib, "folly")