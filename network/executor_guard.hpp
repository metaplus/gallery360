#pragma once

namespace net
{
    class executor_guard
    {
        boost::thread_group& thread_group_;
        std::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> executor_guard_;
        std::shared_ptr<std::once_flag> flag_;

    public:
        executor_guard(boost::thread_group& threads, boost::asio::io_context& context)
            : thread_group_(threads)
            , executor_guard_(std::make_shared<boost::asio::executor_work_guard<
                              boost::asio::io_context::executor_type>>(boost::asio::make_work_guard(context)))
            , flag_(std::make_shared<std::once_flag>())
        {}

        ~executor_guard()
        {
            if (auto const use_count = executor_guard_.use_count(); use_count <= 1)
                reset_guard_and_wait();
        }

        void reset_guard()
        {
            if (!executor_guard_) return;
            executor_guard_->reset();
            executor_guard_->get_executor().context().stop();
            executor_guard_ = nullptr;
        }

        void reset_guard_and_wait()
        {
            reset_guard();
            std::call_once(
                *flag_, [this]
                {
                    if (std::uncaught_exceptions())
                    {
                        fmt::print("executor_guard: destructor exception count: {}\n", std::uncaught_exceptions());
                        thread_group_.interrupt_all();
                    } else
                    {
                        assert(!thread_group_.is_this_thread_in());
                        thread_group_.join_all();
                    }
                });
        }

        boost::asio::io_context::executor_type executor() const noexcept
        {
            return executor_guard_->get_executor();
        }

        boost::asio::io_context& context() const noexcept
        {
            return executor_guard_->get_executor().context();
        }
    };

    static_assert(std::is_copy_constructible<executor_guard>::value);
    static_assert(std::is_move_constructible<executor_guard>::value);
    static_assert(!std::is_copy_assignable<executor_guard>::value);
    static_assert(!std::is_move_assignable<executor_guard>::value);
}
