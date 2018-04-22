#pragma once

namespace core
{
    int inspect_exception(const std::exception & e);

    class aborted_error : protected std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
        using runtime_error::operator=;
        explicit aborted_error(std::string_view desc = ""sv);
        const char* what() const override;
    };

    class null_pointer_error : protected std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
        using runtime_error::operator=;
        explicit null_pointer_error(std::string_view desc = ""sv);
        const char* what() const override;
    };

    class dangling_pointer_error : protected std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
        using runtime_error::operator=;
        explicit dangling_pointer_error(std::string_view desc = ""sv);
        const char* what() const override;
    };

    class not_implemented_error : protected std::logic_error
    {
    public:
        using logic_error::logic_error;
        using logic_error::operator=;
        explicit not_implemented_error(std::string_view desc = ""sv);
        const char* what() const override;
    };
}   