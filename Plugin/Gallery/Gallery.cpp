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
        dll::media_create();
        dll::ipc_create();
        auto msg_time = dll::timer_elapsed();
        auto msg_body = ipc::message::info_launch{ "info_launch"s };
        dll::ipc_async_send(ipc::message{ std::move(msg_body), std::move(msg_time) });
#ifdef NDEBUG
    }
    catch (...) { return false; }
#endif
    return true;
}
void GlobalRelease()
{
#ifdef NDEBUG
    try
    {
#endif
        auto msg_time = dll::timer_elapsed();
        auto msg_body = ipc::message::info_exit{ "info_exit"s };
        dll::ipc_async_send(ipc::message{ std::move(msg_body), std::move(msg_time) });
        std::this_thread::yield();
        dll::media_release();       //consider reverse releasing order
        dll::ipc_release();         //perhaps construct RAII guard inside current TU
#ifdef NDEBUG
    }
    catch (...)
    {
        dll::media_clear();
    }
#endif
}
