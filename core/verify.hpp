#pragma once

namespace core
{
    namespace detail
    {
        [[noreturn]] inline void verify_one(std::nullptr_t) {
            throw_with_stacktrace(null_pointer_error{ "null pointer" });
        }

        template<typename Pointee>
        void verify_one(Pointee* const& ptr) {
            if (!ptr)
                throw_with_stacktrace(
                    dangling_pointer_error{ "dangling pointer, pointer type: " +
                    boost::typeindex::type_id<Pointee>().pretty_name() });
        }

        template<
            typename Arithmetic,
            typename = std::enable_if_t<std::is_arithmetic<Arithmetic>::value>
        >
            void verify_one(Arithmetic number) {
            if (std::is_integral<Arithmetic>::value
                && std::is_signed<Arithmetic>::value && number < 0)
                throw_with_stacktrace(std::out_of_range{ "negative value" });
        }

        inline void verify_one(bool condition) {
            if (!condition)
                throw_with_stacktrace(std::logic_error{ "condition false" });
        }
    }

    template<typename ...Predicates>
    void verify(const Predicates& ...preds) {
        (..., detail::verify_one(preds));
    }

    class check_result
    {
        std::any expect_;

    public:
        template<typename U>
        [[noreturn]] void operator<<(U&& result) {
            if (expect_.has_value()) {
                auto&& expect_value = std::any_cast<const U&>(result);
                assert(std::equal_to<>{}(std::forward<U>(result), expect_value));
                expect_.reset();
            } else {
                using native_type = typename std::decay<U>::type;
                if constexpr (std::is_pointer<native_type>::value) {
                    assert(result != nullptr);
                } else if constexpr (std::is_same<bool, native_type>::value) {
                    assert(result);
                } else if constexpr (std::is_integral<native_type>::value && std::is_signed<native_type>::value) {
                    assert(result >= 0);
                } else {
                    static_assert(!std::is_null_pointer<native_type>::value);
                    static_assert(!std::is_floating_point<native_type>::value);
                    throw unreachable_execution_error{ __PRETTY_FUNCTION__ };
                }
            }
        }

        template<typename ...Types>
        [[noreturn]] void operator<<(std::tuple<Types...>&& result_tuple) {
            std::apply(
                [this](auto&& ...results) {
                    operator<<(std::forward<decltype(results)>(results)...);
                }, std::move(result_tuple));
        }

        template<typename U>
        check_result& operator[](U&& expect) {
            expect_.emplace<U>(std::forward<U>(expect));
            return *this;
        }
    };

    static thread_local check_result check;
}
