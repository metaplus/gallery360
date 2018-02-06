#include "stdafx.h"
#include "interface.h"
namespace
{
    using namespace std::chrono;
    high_resolution_clock::time_point time_base = high_resolution_clock::now();
}
void dll::timer_startup()
{
    time_base = high_resolution_clock::now();
#ifndef NDEBUG
    static auto debug_counter = 0;
    if (++debug_counter > 1)
        throw std::runtime_error{""};
#endif
}
high_resolution_clock::duration dll::timer_elapsed()
{
    return high_resolution_clock::now() - time_base;
}
