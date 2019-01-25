#include "stdafx.h"
#include "database.sqlite.h"

using sqlite_orm::make_storage;
using sqlite_orm::make_table;
using sqlite_orm::make_column;
using sqlite_orm::autoincrement;
using sqlite_orm::primary_key;

inline namespace plugin
{
    auto make_sqlite_storage = [](const std::string& file_name, const std::string& table_name) {
        return make_storage(
            file_name,
            make_table(
                table_name,
                make_column("id", &core::trace_event::id, autoincrement(), primary_key()),
                make_column("instance", &core::trace_event::instance),
                make_column("instance_id", &core::trace_event::instance_id),
                make_column("event", &core::trace_event::event),
                make_column("event_id", &core::trace_event::event_id),
                make_column("event_extension", &core::trace_event::event_extension))
        );
    };

    using database_storage = std::invoke_result<decltype(make_sqlite_storage),
                                                const std::string&, const std::string&>::type;

    static_assert(std::is_move_constructible<database_storage>::value);
    static_assert(!std::is_move_assignable<database_storage>::value);

    struct database::impl final
    {
        std::optional<database_storage> sink_storage;
        moodycamel::BlockingConcurrentQueue<core::trace_event> sink_event_queue;
    };
}
