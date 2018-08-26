#pragma once

namespace core
{
	inline std::string time_string(std::string_view tformat = "%c"sv,
								   std::tm*(*tfunc)(std::time_t const*) = &std::localtime) {
		// auto const time_tmt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		auto const time_tmt = std::time(nullptr);
		auto const time_tm = tfunc(&time_tmt);
		return fmt::format("{}", std::put_time(time_tm, tformat.data()));
	}

	template<auto Begin, auto End, auto Span = 1>
	constexpr auto range_sequence() {
		static_assert(std::is_same<decltype(Begin), decltype(End)>::value);
		static_assert(Begin != End && Span != 0 && ((Begin < End) ^ (Span < 0)));
		constexpr auto count = std::divides<void>{}(End - Begin + Span, Span);
		return meta::sequence_plus<Begin>(meta::sequence_multiply<Span>(std::make_integer_sequence<decltype(count), count>{}));
	}

	template<auto Begin, auto End, auto Span = 1>
	constexpr auto range() {
		return meta::make_array(range_sequence<Begin, End, Span>());
	}

	namespace literals
	{
		constexpr size_t operator""_kbyte(size_t const n) {
			return n * 1024;
		}

		constexpr size_t operator""_mbyte(size_t const n) {
			return n * 1024 * 1024;
		}

		constexpr size_t operator""_gbyte(size_t const n) {
			return n * 1024 * 1024 * 1024;
		}

		template<typename Represent, typename Period>
		std::ostream& operator<<(std::ostream& os, std::chrono::duration<Represent, Period> const& dura) {
			using namespace std::chrono;
			return
				dura < 1us ? os << duration_cast<duration<double, std::nano>>(dura).count() << "ns" :
				dura < 1ms ? os << duration_cast<duration<double, std::micro>>(dura).count() << "us" :
				dura < 1s ? os << duration_cast<duration<double, std::milli>>(dura).count() << "ms" :
				dura < 1min ? os << duration_cast<duration<double>>(dura).count() << "s" :
				dura < 1h ? os << duration_cast<duration<double, std::ratio<60>>>(dura).count() << "min" :
				os << duration_cast<duration<double, std::ratio<3600>>>(dura).count() << "h";
		}
	}

	inline size_t count_entry(std::filesystem::path const& directory) {
		// non-recursive version, regardless of symbolic link
		const std::filesystem::directory_iterator iterator{ directory };
		return std::distance(begin(iterator), end(iterator));
	}

	template<typename T>
	std::reference_wrapper<T> make_null_reference_wrapper() noexcept {
		static void* null_pointer = nullptr;
		return std::reference_wrapper<T>{
			*reinterpret_cast<std::add_pointer_t<std::decay_t<T>>&>(null_pointer)
		};
	}

	// enable DefaultConstructible
	template<typename T>
	class reference : public std::reference_wrapper<T>
	{
	public:
		using std::reference_wrapper<T>::reference_wrapper;
		using std::reference_wrapper<T>::operator=;
		using std::reference_wrapper<T>::operator();

		reference() noexcept
			: std::reference_wrapper<T>(core::make_null_reference_wrapper<T>()) {}
	};

	inline namespace tag //  tag dispatching usage, clarify semantics
	{
		inline constexpr struct use_future_t {} use_future;
		inline constexpr struct use_recursion_t {} use_recursion;
		inline constexpr struct as_default_t {} as_default;
		inline constexpr struct as_stacktrace_t {} as_stacktrace;
		inline constexpr struct as_element_t {} as_element;
		inline constexpr struct as_view_t {} as_view;
		inline constexpr struct as_observer_t {} as_observer;
		inline constexpr struct defer_construct_t {} defer_construct;
		inline constexpr struct defer_execute_t {} defer_execute;
		inline constexpr struct defer_destruct_t {} defer_destruct;
	}

	namespace v3
	{
		namespace detail
		{
			template<typename T, typename ...Types>
			auto hash_value_tuple(T const& head, Types const& ...tails) noexcept;

			template<typename T>
			std::tuple<size_t> hash_value_tuple(T const& head) noexcept {
				return std::make_tuple(std::hash<T>{}(head));
			}

			template<typename T, typename U>
			std::tuple<size_t, size_t> hash_value_tuple(std::pair<T, U> const& head) noexcept {
				return hash_value_tuple(head.first, head.second);
			}

			template<typename ...TupleTypes>
			auto hash_value_tuple(std::tuple<TupleTypes...> const& head) noexcept {
				return hash_value_tuple(std::get<TupleTypes>(head)...);
			}

			template<typename T, typename ...Types>
			auto hash_value_tuple(T const& head, Types const& ...tails) noexcept {
				return std::tuple_cat(hash_value_tuple(head), hash_value_tuple(tails...));
			}
		}

		template<typename ...Types>
		size_t hash_value_from(Types const& ...args) noexcept {
			static_assert(sizeof...(Types) > 0);
			const auto tuple = detail::hash_value_tuple(args...);
			return std::hash<std::string_view>{}(std::string_view{ reinterpret_cast<const char*>(&tuple), sizeof tuple });
		}

		template<typename ...Types>
		struct byte_hash
		{
			size_t operator()(Types const& ...args) noexcept {
				return hash_value_from(args);
			}
		};

		template<>
		struct byte_hash<void>
		{
			template<typename ...Types>
			size_t operator()(Types const& ...args) const noexcept {
				return hash_value_from(args...);
			}
		};
	}

	using v3::byte_hash;
	using v3::hash_value_from;

	template<typename Hash>
	struct deref_hash
	{
		// smart pointer or iterator
		template<typename Handle>
		size_t operator()(Handle const& handle) const {
			return Hash{}(*handle);
		}
	};

	template<typename Handle>
	decltype(auto) get_pointer(Handle&& handle, std::enable_if_t<meta::has_operator_dereference<Handle>::value>* = nullptr) {
		return std::forward<Handle>(handle).operator->();
	}

	template<typename Pointee>
	Pointee* const& get_pointer(Pointee* const& handle) noexcept {
		return handle;
	}

	template<typename T>
	[[nodiscard]] constexpr std::remove_const_t<T>& as_mutable(T& object) noexcept {
		return const_cast<std::remove_const_t<T>&>(object);
	}

	template<typename T>
	[[nodiscard]] constexpr T& as_mutable(const T* ptr) noexcept {
		assert(ptr != nullptr);
		return const_cast<T&>(*ptr);
	}

	template<typename T>
	void as_mutable(T const&&) = delete;

	template<typename T, typename U>
	constexpr bool address_same(T const& x, U const& y) noexcept {
		return std::addressof(x) == std::addressof(y);
	}

	template<typename Enum>
	constexpr std::underlying_type_t<Enum> underlying(Enum const& enumeration) noexcept {
		static_assert(std::is_enum<Enum>::value);
		return static_cast<std::underlying_type_t<Enum>>(enumeration);
	}

	template<typename EnumT, typename EnumU>
	constexpr bool underlying_same(EnumT const& et, EnumU const& eu) noexcept {
		return std::equal_to<>{}(underlying(et), underlying(eu));
	}
}
