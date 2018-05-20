#pragma once

namespace net
{
    namespace base
    {
        class context_base
        {
        protected:
            // static std::string& delim_suffix() noexcept
            // {
            //     return delim_suffix_;
            // }
            //
            // static bool delim_suffix_valid() noexcept
            // {
            //     return !delim_suffix_.empty();
            // }

            const std::string& next_delim()
            {
                const auto delim_index = std::atomic_fetch_add(&count_, 1);
                delim_.clear();
                delim_.reserve(delim_prefix.size() + sizeof delim_index + delim_suffix.size());
                // return delim_ = delim_prefix + std::to_string(delim_index) + delim_suffix;
                return delim_.append(delim_prefix).append(reinterpret_cast<const char*>(&delim_index), sizeof delim_index).append(delim_suffix);
            }

            const std::string& prev_delim() const noexcept
            {
                return delim_;
            }

            bool disposing_ = false;
            std::atomic<int64_t> count_ = 0;

        private:
            inline static const std::string delim_prefix{ "[DelimPrefix]" };
            inline static const std::string delim_suffix{ "[DelimSuffix]" };
            std::string delim_;
        };
    }
}