#pragma once

namespace core
{
    int inspect_exception(std::exception const& exception, bool print_thread = true);

	int inspect_exception(boost::exception const& exception, bool print_thread = true);

    class aborted_error : protected std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
        using runtime_error::operator=;
        explicit aborted_error(std::string_view desc = ""sv);
        char const* what() const override;
    };

    class null_pointer_error : protected std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
        using runtime_error::operator=;
        explicit null_pointer_error(std::string_view desc = ""sv);
        char const* what() const override;
    };

    class dangling_pointer_error : protected std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
        using runtime_error::operator=;
        explicit dangling_pointer_error(std::string_view desc = ""sv);
        char const* what() const override;
    };

    class not_implemented_error : protected std::logic_error
    {
    public:
        using logic_error::logic_error;
        using logic_error::operator=;
        explicit not_implemented_error(std::string_view desc = ""sv);
        char const* what() const override;
    };

    class already_exist_error : protected std::logic_error
    {
    public:
        using logic_error::logic_error;
        using logic_error::operator=;
        explicit already_exist_error(std::string_view desc = ""sv);
        char const* what() const override;
    };

    class unreachable_execution_branch : protected std::logic_error
    {
    public:
        using logic_error::logic_error;
        using logic_error::operator=;
        explicit unreachable_execution_branch(std::string_view desc = ""sv);
        char const* what() const override;
    };
}   