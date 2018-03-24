#pragma once

namespace core
{
    int inspect_exception(const std::exception & e);

    class aborted_error : protected std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
        explicit aborted_error(std::string_view desc = ""sv);
        const char* what() const override;
    };

    class null_pointer_error : protected std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
        explicit null_pointer_error(std::string_view desc = ""sv);
        const char* what() const override;
    };

    class dangling_pointer_error : protected std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
        explicit dangling_pointer_error(std::string_view desc = ""sv);
        const char* what() const override;
    };

    class not_implemented_error : protected std::logic_error
    {
    public:
        using logic_error::logic_error;
        explicit not_implemented_error(std::string_view desc = ""sv);
        const char* what() const override;
    };
}   