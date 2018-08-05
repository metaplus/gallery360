#pragma once

namespace media
{
    struct io_context_base
    {
        using read_context = folly::Function<int(uint8_t*, int)>;
        using write_context = folly::Function<int(uint8_t*, int)>;
        using seek_context = folly::Function<int64_t(int64_t, int)>;
    };

    struct io_base
    {
        virtual ~io_base() = default;
        virtual int read(uint8_t* buffer, int size) = 0;
        virtual int write(uint8_t* buffer, int size) = 0;
        virtual int64_t seek(int64_t offset, int whence) = 0;
        virtual bool readable() = 0;
        virtual bool writable() = 0;
        virtual bool seekable() = 0;
    };

    struct cursor_base
    {
        using buffer_type = boost::beast::multi_buffer;
        using const_iterator = buffer_type::const_buffers_type::const_iterator;
    };

    struct cursor
    {
        using buffer_type = boost::beast::multi_buffer;
        using const_iterator = buffer_type::const_buffers_type::const_iterator;

        const_iterator const buffer_begin;
        const_iterator const buffer_end;
        const_iterator buffer_iter;
        int64_t buffer_offset = 0;
        int64_t sequence_offset = 0;
        std::vector<int64_t> const buffer_sizes;

        explicit cursor(buffer_type const& buffer);
        int64_t seek_sequence(int64_t seek_offset);
        int64_t buffer_size() const;
        int64_t sequence_size() const;
    };

    struct generic_cursor final : io_base, io_context_base
    {
        read_context reader;
        write_context writer;
        seek_context seeker;

        explicit generic_cursor(
            read_context&& rfunc = nullptr,
            write_context&& wfunc = nullptr,
            seek_context&& sfunc = nullptr);
        ~generic_cursor() = default;

        int read(uint8_t* buffer, int size) override;
        int write(uint8_t* buffer, int size) override;
        int64_t seek(int64_t offset, int whence) override;
        bool readable() override;
        bool writable() override;
        bool seekable() override;

        static std::shared_ptr<generic_cursor> create(read_context&& rfunc = nullptr,
                                                      write_context&& wfunc = nullptr,
                                                      seek_context&& sfunc = nullptr);
    };

    struct random_access_curser final : io_base, cursor
    {
        explicit random_access_curser(buffer_type const& buffer);
        ~random_access_curser() override = default;

        int read(uint8_t* buffer, int expect_size) override;
        int write(uint8_t* buffer, int size) override;
        int64_t seek(int64_t seek_offset, int whence) override;
        bool readable() override;
        bool writable() override;
        bool seekable() override;

        static std::shared_ptr<random_access_curser> create(buffer_type const& buffer);
    };

    struct forward_cursor_stream final : io_base, cursor_base
    {
        using buffer_supplier = folly::Function<boost::future<buffer_type>()>;

        std::optional<buffer_type> current_buffer;
        std::shared_ptr<random_access_curser> io_base;
        buffer_supplier on_future_buffer;
        boost::future<buffer_type> future_buffer;
        bool eof = false;

        forward_cursor_stream(buffer_supplier&& supplier)
            : on_future_buffer(std::move(supplier))
            , future_buffer(on_future_buffer())
        {}

        ~forward_cursor_stream() override = default;

        bool shift_next_buffer()
        {
            try
            {
                current_buffer = future_buffer.get();  // exception point
                io_base = random_access_curser::create(current_buffer.value());
                future_buffer = on_future_buffer();
            }
            catch (...)
            {
                return false;
            }
            return true;
        }

        int read(uint8_t* buffer, int expect_size) override
        {
            if (eof || !current_buffer.has_value() && !shift_next_buffer())
            {
                fmt::print("----- eof\n");
                return AVERROR_EOF;
            }
            auto total_read_size = 0;
            auto increment_read_size = 0;
            fmt::print("----- start expect {}\n", expect_size);
            while (total_read_size < expect_size)
            {
                increment_read_size = io_base->read(buffer + total_read_size, expect_size - total_read_size);
                if (increment_read_size == AVERROR_EOF)
                {
                    fmt::print("rebuild\n");
                    if (!shift_next_buffer())
                    {
                        eof = true;
                        break;
                    }
                    increment_read_size = io_base->read(buffer + total_read_size, expect_size - total_read_size);
                }
                assert(increment_read_size > 0);
                total_read_size += increment_read_size;
            }
            fmt::print("----- end total read {}\n", total_read_size);
            return total_read_size;
        }
        int write(uint8_t* buffer, int size) override { throw 1; }
        int64_t seek(int64_t seek_offset, int whence) override { throw 1; }
        bool readable() override { return true; }
        bool writable() override { return false; }
        bool seekable() override { return false; }

        static std::shared_ptr<forward_cursor_stream> create(buffer_supplier&& supplier)
        {
            return std::make_shared<forward_cursor_stream>(std::move(supplier));
        }
    };
}