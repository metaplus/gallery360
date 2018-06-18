#include "stdafx.h"
#include "exception.h"

int core::inspect_exception(std::exception const& exception, bool const print_thread)
{
	if (print_thread) fmt::print(std::cerr, "thread@{} ", boost::this_thread::get_id());
	fmt::print(std::cerr, "what: {}\n", exception.what());
	try
	{
		std::rethrow_if_nested(exception);
	} catch (const std::exception& exception1)
	{
		return inspect_exception(exception1, false);
	} catch (const boost::exception& ex2)
	{
		return inspect_exception(ex2, false);
	} catch (...)
	{
		fmt::print(std::cerr, "caught: nonstandard exception");
	}
	std::cerr << std::endl;
	return EXIT_FAILURE;
}

int core::inspect_exception(boost::exception const& ex, bool const print_thread)
{
	if (print_thread) fmt::print(std::cerr, "thread@{} ", boost::this_thread::get_id());
	fmt::print(std::cerr, "diagnostic: {}\n", boost::diagnostic_information(ex));
	return EXIT_FAILURE;
}

namespace
{
	template<typename T>
	char const* deduce_error_cstring(char const* cstr, T const* = nullptr)
	{
		static thread_local std::string local_type_name;
		// return std::strlen(cstr) != 0 ? cstr : (local_type_name = core::type_shortname<std::remove_cv_t<T>>()).c_str();
		return !std::string_view{ cstr }.empty() ? cstr : (local_type_name = core::type_shortname<std::remove_cv_t<T>>()).c_str();
	}
}

core::aborted_error::aborted_error(std::string_view const desc)
	: runtime_error(desc.data())
{}

char const* core::aborted_error::what() const
{
	return deduce_error_cstring(what(), this);
}

core::null_pointer_error::null_pointer_error(std::string_view desc)
	: runtime_error(desc.data())
{}

char const* core::null_pointer_error::what() const
{
	return deduce_error_cstring(what(), this);
}

core::dangling_pointer_error::dangling_pointer_error(std::string_view desc)
	: runtime_error(desc.data())
{}

char const* core::dangling_pointer_error::what() const
{
	return deduce_error_cstring(what(), this);
}

core::not_implemented_error::not_implemented_error(std::string_view desc)
	: logic_error(desc.data())
{}

char const* core::not_implemented_error::what() const
{
	return deduce_error_cstring(what(), this);
}

core::already_exist_error::already_exist_error(std::string_view desc)
	: logic_error(desc.data())
{}

char const* core::already_exist_error::what() const
{
	return deduce_error_cstring(what(), this);
}

core::unreachable_execution_branch::unreachable_execution_branch(std::string_view desc)
	: logic_error(desc.data())
{}

char const* core::unreachable_execution_branch::what() const
{
	return deduce_error_cstring(what(), this);
}
