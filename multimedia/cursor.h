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
        io_base() = default;
        io_base(const io_base&) = default;
        io_base(io_base&&) noexcept = default;
        io_base& operator=(const io_base&) = default;
        io_base& operator=(io_base&&) noexcept = default;
        virtual ~io_base() = default;
        virtual int read(uint8_t* buffer, int size) = 0;
        virtual int write(uint8_t* buffer, int size) = 0;
        virtual int64_t seek(int64_t offset, int whence) = 0;
        virtual bool readable() = 0;
        virtual bool writable() = 0;
        virtual bool seekable() = 0;
        virtual bool available() { throw core::not_implemented_error{ __FUNCSIG__ }; }
        virtual int64_t consume_size() { throw core::not_implemented_error{ __FUNCSIG__ }; }
        virtual int64_t remain_size() { throw core::not_implemented_error{ __FUNCSIG__ }; }
    };

    using boost::asio::const_buffer;
    using boost::beast::multi_buffer;
    using const_buffer_iterator = multi_buffer::const_buffers_type::const_iterator;
    using mutable_buffer_iterator = multi_buffer::mutable_buffers_type::const_iterator;

    struct cursor
    {
        const_buffer_iterator const buffer_begin;
        const_buffer_iterator const buffer_end;
        const_buffer_iterator buffer_iter;
        int64_t buffer_offset = 0;
        int64_t sequence_offset = 0;
        std::vector<int64_t> const buffer_sizes;

        explicit cursor(const multi_buffer & buffer);
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

    struct random_access_cursor final : io_base, cursor
    {
        explicit random_access_cursor(const multi_buffer& buffer);
        ~random_access_cursor() override = default;

        int read(uint8_t* buffer, int expect_size) override;
        int write(uint8_t* buffer, int size) override;
        int64_t seek(int64_t seek_offset, int whence) override;
        bool readable() override;
        bool writable() override;
        bool seekable() override;

        static std::shared_ptr<random_access_cursor> create(const multi_buffer& buffer);
    };

    class buffer_list_cursor final : public io_base
    {
        std::list<const_buffer> buffer_list_;
        std::list<const_buffer>::iterator buffer_iter_;
        int64_t offset_ = 0;
        int64_t full_offset_ = 0;
        int64_t full_size_ = 0;

    public:
        explicit buffer_list_cursor(std::list<const_buffer> buffer_list);
        int read(uint8_t* buffer, int expect_size) override;
        int write(uint8_t* buffer, int size) override;
        int64_t seek(int64_t seek_offset, int whence) override;
        bool readable() override;
        bool writable() override;
        bool seekable() override;
        bool available() override;
        int64_t consume_size() override;
        int64_t remain_size() override;

        static std::shared_ptr<buffer_list_cursor> create(const multi_buffer& buffer);
    };

    struct forward_stream_cursor final : io_base
    {
        using buffer_supplier = folly::Function<boost::future<multi_buffer>()>;

        std::optional<multi_buffer> current_buffer;
        std::shared_ptr<random_access_cursor> io_base;
        buffer_supplier on_future_buffer;
        boost::future<multi_buffer> future_buffer;
        bool eof = false;

        explicit forward_stream_cursor(buffer_supplier&& supplier);
        ~forward_stream_cursor() override = default;

        bool shift_next_buffer();

        int read(uint8_t* buffer, int expect_size) override;
        int write(uint8_t* buffer, int size) override { throw core::not_implemented_error{ "TBD" }; }
        int64_t seek(int64_t seek_offset, int whence) override { throw core::not_implemented_error{ "TBD" }; }
        bool readable() override { return true; }
        bool writable() override { return false; }
        bool seekable() override { return false; }

        static std::shared_ptr<forward_stream_cursor> create(buffer_supplier&& supplier);
    };
}
