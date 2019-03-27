#pragma once
#include <sqlite_orm/sqlite_orm.h>
#include <absl/time/clock.h>
#include <absl/time/time.h>
#include <absl/strings/str_join.h>
#include <blockingconcurrentqueue.h>
#include <readerwriterqueue/readerwriterqueue.h>
#include <optional>
#include <filesystem>

inline namespace plugin
{
    struct trace_event final
    {
        int64_t id = 0;
        std::string date_time;
        std::string instance;
        int instance_id = 0;
        std::string event;
        int event_id = 0;
    };

    auto make_sqlite_storage = [](const std::string& file_name, const std::string& table_name) {
        using sqlite_orm::make_storage;
        using sqlite_orm::make_table;
        using sqlite_orm::make_column;
        using sqlite_orm::autoincrement;
        using sqlite_orm::primary_key;
        return make_storage(
            file_name,
            make_table(
                table_name,
                make_column("id", &trace_event::id, autoincrement(), primary_key()),
                make_column("date_time", &trace_event::date_time),
                make_column("instance", &trace_event::instance),
                make_column("instance_id", &trace_event::instance_id),
                make_column("event", &trace_event::event),
                make_column("event_id", &trace_event::event_id)));
    };

    using database_storage = std::invoke_result<decltype(make_sqlite_storage),
                                                const std::string&, const std::string&>::type;
    using std::literals::operator ""sv;

    static_assert(std::is_move_constructible<database_storage>::value);
    static_assert(!std::is_move_assignable<database_storage>::value);

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

        std::filesystem::path file_path;
        std::optional<database_storage> sink_storage;
        std::optional<absl::TimeZone> time_zone;
        moodycamel::BlockingConcurrentQueue<trace_event> sink_event_queue;
        moodycamel::ReaderWriterQueue<command> command_queue{ 3 };
    public:

        database() = default;

        explicit database(std::filesystem::path file_path,
                          std::string_view infix = "adaptive") {
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
            time_zone = std::move(time_zone);
            file_path = std::move(file_path);
            sink_storage
                .emplace(make_sqlite_storage(file_name, table_name))
                .sync_schema(true);
        }

        database(const database&) = default;
        database(database&&) noexcept = default;
        database& operator=(const database&) = default;
        database& operator=(database&&) noexcept = default;
        ~database() = default;

        void submit_entry(trace_event&& event) {
            sink_event_queue.enqueue(std::move(event));
        }

        void submit_command(command command) {
            command_queue.enqueue(command);
        }

        int64_t insert_entry_persistently() {
            const auto bulk_size = 64;
            int64_t sink_count = 0;
            std::vector<trace_event> entries(bulk_size);
            moodycamel::ConsumerToken token{ sink_event_queue };
            auto* stop_command = command_queue.peek();
            auto dequeue_size = 0ui64;
            const auto insert_event_into_storage = [&] {
                sink_storage->insert_range(entries.begin(), entries.end());
                sink_count += dequeue_size;
            };
            while (!stop_command) {
                dequeue_size = sink_event_queue
                    .wait_dequeue_bulk(token, entries.begin(), bulk_size);
                insert_event_into_storage();
                stop_command = command_queue.peek();
            }
            while ((dequeue_size = sink_event_queue
                .try_dequeue_bulk(token, entries.begin(), bulk_size))) {
                insert_event_into_storage();
            }
            return sink_count;
        }

    private:
    };
}
