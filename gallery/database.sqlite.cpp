#include "stdafx.h"
#include "database.sqlite.h"
#include <absl/time/clock.h>
#include <absl/time/time.h>
#include <absl/strings/str_join.h>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/begin_end.hpp>

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
                make_column("date_time", &core::trace_event::date_time),
                make_column("instance", &core::trace_event::instance),
                make_column("instance_id", &core::trace_event::instance_id),
                make_column("event", &core::trace_event::event),
                make_column("event_id", &core::trace_event::event_id)));
    };

    using database_storage = std::invoke_result<decltype(make_sqlite_storage),
                                                const std::string&, const std::string&>::type;

    static_assert(std::is_move_constructible<database_storage>::value);
    static_assert(!std::is_move_assignable<database_storage>::value);

    struct database::impl final
    {
        std::filesystem::path file_path;
        std::optional<database_storage> sink_storage;
        std::optional<absl::TimeZone> time_zone;
        moodycamel::BlockingConcurrentQueue<core::trace_event> sink_event_queue;
        moodycamel::ReaderWriterQueue<command> command_queue{ 3 };
    };

    database::database(std::filesystem::path file_path, std::string_view infix)
        : impl_{ std::make_shared<impl>() } {
        if (!is_regular_file(file_path)) {
            throw std::filesystem::filesystem_error{
                file_path.string(),
                std::make_error_code(std::errc::no_such_file_or_directory)
            };
        }
        auto time_zone = absl::FixedTimeZone(8 * 60 * 60);
        const auto file_name = file_path.generic_string();
        const auto table_name = absl::StrJoin({
            "trace_event"sv, infix,
            std::string_view{ FormatTime("%Y%m%d.%H%M%S", absl::Now(), time_zone) }
        }, ".");
        impl_->time_zone = std::move(time_zone);
        impl_->file_path = std::move(file_path);
        impl_->sink_storage
             .emplace(make_sqlite_storage(file_name, table_name))
             .sync_schema(true);
    }

    void database::submit_entry(core::trace_event&& event) const {
        impl_->sink_event_queue.enqueue(std::move(event));
    }

    void database::submit_command(command command) const {
        impl_->command_queue.enqueue(command);
    }

    int64_t database::insert_entry_persistently() const {
        const auto bulk_size = 64;
        int64_t sink_count = 0;
        std::vector<core::trace_event> entries(bulk_size);
        moodycamel::ConsumerToken token{ impl_->sink_event_queue };
        auto* stop_command = impl_->command_queue.peek();
        auto dequeue_size = 0ui64;
        const auto insert_event_into_storage = [&] {
            impl_->sink_storage->insert_range(entries.begin(), entries.end());
            sink_count += dequeue_size;
        };
        while (!stop_command) {
            dequeue_size = impl_->sink_event_queue
                                .wait_dequeue_bulk(token, entries.begin(), bulk_size);
            insert_event_into_storage();
            stop_command = impl_->command_queue.peek();
        }
        while ((dequeue_size = impl_->sink_event_queue
                                    .try_dequeue_bulk(token, entries.begin(), bulk_size))) {
            insert_event_into_storage();
        }
        return sink_count;
    }
}
