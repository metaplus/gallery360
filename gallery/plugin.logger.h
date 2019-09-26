#pragma once
#include "core/core.h"
#include <spdlog/logger.h>
#include <boost/container/flat_map.hpp>
#include <spdlog/spdlog.h>
#include <optional>
#include <filesystem>
#include <absl/strings/str_cat.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <range/v3/action/transform.hpp>

namespace plugin
{
    enum class logger_type
    {
        plugin, update, render,
        decode, camera,
        other, all,
    };

    class logger_manager final
    {
        boost::container::flat_map<logger_type, std::string> logger_filename_prefix_;
        std::array<std::shared_ptr<spdlog::logger>,
                   static_cast<size_t>(logger_type::all)> logger_table_;
        std::optional<bool> enable_log_;
        std::optional<std::filesystem::path> directory_;

    public:
        logger_manager()
            : logger_filename_prefix_{
                { logger_type::plugin, "plugin" },
                { logger_type::update, "plugin.update" },
                { logger_type::render, "plugin.render" },
                { logger_type::decode, "worker.decode" },
                { logger_type::camera, "camera" },
                { logger_type::other, "other" },
            } {
            spdlog::drop_all();
        }

        ~logger_manager() {
            logger_table_ |= ranges::action::transform(
                [](decltype(logger_table_)::reference logger) {
                    if (logger) {
                        logger->flush();
                    }
                    return nullptr;
                });
            spdlog::drop_all();
        }

        logger_manager(const logger_manager&) = delete;
        logger_manager(logger_manager&&) = delete;
        logger_manager& operator=(const logger_manager&) = delete;
        logger_manager& operator=(logger_manager&&) = delete;

        logger_manager& enable_log(bool enable) {
            enable_log_.emplace(enable);
            return *this;
        }

        logger_manager& directory(std::filesystem::path directory) {
            if (enable_log_.value_or(false) && !is_directory(directory)) {
                using namespace std::string_literals;
                throw std::filesystem::filesystem_error{
                    "logger_manager set directory failure"s,
                    directory, std::make_error_code(std::errc::not_a_directory)
                };
            }
            directory_.emplace(std::move(directory));
            return *this;
        }

        std::shared_ptr<spdlog::logger>& get(logger_type type) {
            auto& logger = logger_table_[static_cast<size_t>(type)];
            if (logger == nullptr) {
                auto& logger_name = logger_filename_prefix_.at(type);
                logger = make_logger(logger_name);
            }
            return logger;
        }

    private:
        std::shared_ptr<spdlog::logger> make_logger(const std::string& logger_name) {
            if (!enable_log_.value_or(false)) {
                return spdlog::null_logger_st(logger_name);
            }
            auto sink_path = directory_.value() / absl::StrCat(logger_name, ".log");
            return core::make_async_logger(
                logger_name, std::make_shared<spdlog::sinks::basic_file_sink_mt>(sink_path.string()));
        }
    };
}
