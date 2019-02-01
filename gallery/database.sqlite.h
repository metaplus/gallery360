#pragma once
#include <sqlite_orm/sqlite_orm.h>

inline namespace plugin
{
    class database final
    {
        struct impl;
        enum class command
        {
            execute,
            complete,
            stop_immediately,
            stop_after_complete,
        };
 
        std::shared_ptr<impl> impl_;

    public:
        struct trace_event final
        {
            int64_t id = 0;
            std::string date_time;
            std::string instance;
            int instance_id = 0;
            std::string event;
            int event_id = 0;
        };

        database() = default;
        explicit database(std::filesystem::path file_path,
                          std::string_view infix = "adaptive"sv);
        database(const database&) = default;
        database(database&&) noexcept = default;
        database& operator=(const database&) = default;
        database& operator=(database&&) noexcept = default;
        ~database() = default;

        void submit_entry(trace_event&& event) const;
        void submit_command(command command) const;
        int64_t insert_entry_persistently() const;

    private:
    };
}
