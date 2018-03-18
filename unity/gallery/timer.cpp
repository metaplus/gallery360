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
}

high_resolution_clock::duration dll::timer_elapsed()
{
    return high_resolution_clock::now() - time_base;
}