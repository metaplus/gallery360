#pragma once

namespace core
{
    int inspect_exception(const std::exception & e);

    class force_exit_exception : public std::exception
    {
    public:
        explicit force_exit_exception(std::string_view desc = ""sv);
        const char* what() const override;
    private:
        std::string_view description_;
    };
}