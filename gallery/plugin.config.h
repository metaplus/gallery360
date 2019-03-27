#pragma once
#include <nlohmann/json.hpp>
#include <folly/Uri.h>
#include <optional>
#include <filesystem>
#include <thread>
#include "core/meta/type_trait.hpp"

inline namespace plugin
{
    class config final
    {
    public:
        std::optional<folly::Uri> mpd_uri;
        std::filesystem::path workset_directory;
        std::filesystem::path document_path;
        nlohmann::json document;
        int uri_index = 0;

        struct adaptation
        {
            bool enable = true;
            int algorithm_index = 0;
            int constant_qp = 22;
        } adaptation;

        struct trace
        {
            std::filesystem::path directory;
            std::string prefix;
            bool enable = false;
        } trace;

        struct system
        {
            double predict_degrade_factor = 1;
            struct decode
            {
                size_t capacity = 30;
                bool enable = true;
            } decode;
        } system;

    private:
        template <typename T>
        class alternate final
        {
            T value_ = 0;
            mutable std::optional<T> alternate_;

        public:
            constexpr explicit alternate(const T& value)
                : value_{ value } {}

            template <typename U>
            constexpr alternate& operator=(const U& alternate) {
                if constexpr (meta::is_within<U, std::nullptr_t, std::nullopt_t>::value) {
                    alternate_ = std::nullopt;
                } else {
                    static_assert(std::is_convertible<U, T>::value);
                    alternate_ = alternate;
                }
                return *this;
            }

            constexpr operator T() const {
                return alternate_.value_or(value_);;
            }
        };

    public:
        struct concurrency
        {
            alternate<unsigned> network{ std::thread::hardware_concurrency() };
            alternate<unsigned> executor{ std::thread::hardware_concurrency() };
            alternate<unsigned> decoder{ 2u };
        } concurrency;
    };

    static_assert(std::is_default_constructible<config>::value);

    inline void from_json(const nlohmann::json& json, config& config) {
        json.at("TraceEvent").at("NamePrefix").get_to(config.trace.prefix);
        json.at("TraceEvent").at("Enable").get_to(config.trace.enable);
        json.at("Dash").at("UriIndex").get_to(config.uri_index);
        json.at("Dash").at("Adaptation").at("Enable").get_to(config.adaptation.enable);
        json.at("Dash").at("Adaptation").at("AlgorithmIndex").get_to(config.adaptation.algorithm_index);
        json.at("Dash").at("Adaptation").at("ConstantQP").get_to(config.adaptation.constant_qp);
        config.mpd_uri = folly::Uri{
            json.at("Dash").at("Uri").at(config.uri_index).get<std::string>()
        };
        json.at("System").at("PredictFactor").get_to(config.system.predict_degrade_factor);
        json.at("System").at("DecodeCapacity").get_to(config.system.decode.capacity);
    }
}
