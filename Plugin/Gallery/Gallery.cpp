// Gallery.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "interface.h"

BOOL GlobalCreate()
{
#ifdef NDEBUG                       
    try
    {
#endif
        dll::interprocess_create();
        dll::media_create();
        auto msg_time = dll::timer_elapsed();
        auto msg_body = ipc::message::info_launch{};
        dll::interprocess_async_send(ipc::message{ std::move(msg_body), std::move(msg_time) });
#ifdef NDEBUG
    }
    catch (...) { return false; }
#endif
    return true;
}
void GlobalRelease()
{
        auto msg_time = dll::timer_elapsed();
        auto msg_body = ipc::message::info_exit{};
        dll::interprocess_async_send(ipc::message{ std::move(msg_body), std::move(msg_time) });
        std::this_thread::yield();
        dll::media_release();       
        dll::interprocess_release();         
}
