// Monitor.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
namespace filesystem = std::experimental::filesystem;

int main()
{
    {
        /*
        std::promise<void> signal;
        auto aa = std::thread{[future=signal.get_future()]() mutable{
            auto time_mark = std::chrono::high_resolution_clock::now();
            ipc::channel send_ch{false};
            auto count = -1;
            while (++count != 100) {
                fmt::print("!sending {} message!\n", count);
                auto duration = std::chrono::high_resolution_clock::now() - time_mark;
                send_ch.async_send(vr::Compositor_CumulativeStats{ }, duration );
            }
            fmt::print("!!sending finish!!\n");
            future.get();
            fmt::print("!!sending finish future!!\n");
        }};*/
        std::this_thread::sleep_for(100ms);
        auto bb = std::thread{ []() mutable {
            auto time_mark = std::chrono::high_resolution_clock::now();
            ipc::channel recv_ch{ true };
            auto count = -1;
            while (++count != 100) {
                fmt::print("$receiving {} message$\n", count);
                auto duration = std::chrono::high_resolution_clock::now() - time_mark;
                auto result = recv_ch.async_receive();
                result.first.get();
            }
            fmt::print("$receiving finish$$\n");
            //promise.set_value();
            fmt::print("$$receiving finish promise$$\n");
        } };
        //aa.join();
        bb.join();
    }

    auto dummy2 = 2;

    return 0;
    
}