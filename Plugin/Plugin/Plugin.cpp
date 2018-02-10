// Plugin.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Gallery/interface.h"
#include "Core/guard.h"

namespace filesystem = std::experimental::filesystem;


int main(int argc, char* argv[])
{

    auto aa = std::thread{ []() mutable{
        auto time_mark = std::chrono::high_resolution_clock::now();
        ipc::channel send_ch{ true };
        auto count = -1;
        while (++count != 2000) {
            fmt::print("!sending {} message!\n", count);
            auto duration = std::chrono::high_resolution_clock::now() - time_mark;
            auto timing = vr::Compositor_CumulativeStats{};
            auto msg = ipc::message{ std::move(timing), std::move(duration) };
            send_ch.async_send(timing, duration);
        }
        fmt::print("!!sending finish!!\n");
        std::this_thread::sleep_for(1h);
        fmt::print("!!sending finish future!!\n");
    } };
    aa.join();
}

