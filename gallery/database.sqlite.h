#pragma once
#include <sqlite_orm/sqlite_orm.h>

namespace core
{
    struct trace_event final
    {
        int id = 0;
        std::string instance;
        int instance_id = 0;
        std::string event;
        int event_id = 0;
        std::unique_ptr<std::string> event_extension;
    };
}

inline namespace plugin
{
    class database final 
    {
        struct impl;
        struct impl_deleter final
        {
            void operator()(impl* impl);
        };

        std::shared_ptr<impl> impl_;
    public:

    private:
        database(const std::string& file_name, const std::string& table_name);
    };
}
