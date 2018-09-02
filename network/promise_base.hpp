#pragma once

namespace net
{
    template<typename T>
    class promise_base;
}

namespace detail
{
    using net::promise_base;

    template<typename T, template<typename> typename Promise>
    class promise_impl final : public promise_base<T>
    {
        Promise<T> promise_;

    public:
        explicit promise_impl(Promise<T>&& promise)
            : promise_base<T>()
            , promise_(std::move(promise)) {}

        promise_impl(promise_impl const&) = delete;
        promise_impl(promise_impl&&) noexcept = default;
        promise_impl& operator=(promise_impl const&) = delete;
        promise_impl& operator=(promise_impl&&) noexcept = default;

        void set_value(T&& value) override {
            promise_.set_value(std::move(value));
        }

        void set_exception(std::string_view message) override {
            promise_.set_exception(std::make_exception_ptr(std::runtime_error{ message.data() }));
        }
    };
}

namespace net
{
    template<typename T>
    class promise_base
    {
    public:
        promise_base() = default;
        promise_base(promise_base&&) noexcept = default;
        promise_base& operator=(promise_base&&) noexcept = default;
        promise_base(promise_base const&) = delete;
        promise_base& operator=(promise_base const&) = delete;

        virtual void set_value(T&& value) = 0;
        virtual void set_exception(std::string_view message) = 0;
        virtual ~promise_base() = default;

        template<template<typename> typename Promise>
        static std::unique_ptr<promise_base<T>> from(Promise<T>&& promise) {
            return std::make_unique<detail::promise_impl<T, Promise>>(std::move(promise));
        }
    };

    static_assert(std::is_move_constructible<detail::promise_impl<int, std::promise>>::value);
    static_assert(std::is_move_assignable<detail::promise_impl<int, std::promise>>::value);
}
