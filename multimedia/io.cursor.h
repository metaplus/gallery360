#pragma once
#include <cstdint>
#include <folly/Function.h>
#include <boost/asio/buffer.hpp>
#include <boost/beast/core/multi_buffer.hpp>

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

        virtual bool readable() const;
        virtual bool writable() const;
        virtual bool seekable() const;

        virtual bool available() const;
        virtual int64_t consume_size() const;
        virtual int64_t remain_size() const;
    };

    class cursor_base;

    namespace detail
    {
        using boost::asio::const_buffer;
        using boost::beast::multi_buffer;
        using const_buffer_iterator = multi_buffer::const_buffers_type::const_iterator;
        using mutable_buffer_iterator = multi_buffer::mutable_buffers_type::const_iterator;
    }

    class buffer_list_cursor final : public io_base
    {
        std::list<detail::const_buffer> buffer_list_;
        std::list<detail::const_buffer>::iterator buffer_iterator_;
        int64_t offset_ = 0;
        int64_t full_read_size_ = 0;
        int64_t full_offset_ = 0;
        int64_t full_size_ = 0;
        bool eof_ = false;

    public:
        explicit buffer_list_cursor(std::list<detail::const_buffer> buffer_list);

        int read(uint8_t* buffer, int expect_size) override;
        int write(uint8_t* buffer, int size) override;
        int64_t seek(int64_t seek_offset, int whence) override;

        bool readable() const override;
        bool seekable() const override;

        bool available() const override;
        int64_t consume_size() const override;
        int64_t remain_size() const override;

        static std::unique_ptr<buffer_list_cursor> create(const detail::multi_buffer& buffer);
        static std::unique_ptr<buffer_list_cursor> create(std::list<detail::const_buffer>&& buffer_list);
    };

    struct cursor
    {
        const detail::const_buffer_iterator buffer_begin;
        const detail::const_buffer_iterator buffer_end;
        detail::const_buffer_iterator buffer_iterator;
        int64_t buffer_offset = 0;
        int64_t sequence_offset = 0;
        std::vector<int64_t> const buffer_sizes;

        explicit cursor(const detail::multi_buffer& buffer);

        int64_t seek_sequence(int64_t seek_offset);
        int64_t buffer_size() const;
        int64_t sequence_size() const;
    };

    struct generic_cursor final : io_base, io_context_base
    {
        read_context reader;
        write_context writer;
        seek_context seeker;

        explicit generic_cursor(read_context&& rfunc = nullptr,
                                write_context&& wfunc = nullptr,
                                seek_context&& sfunc = nullptr);
        ~generic_cursor() = default;

        int read(uint8_t* buffer, int size) override;
        int write(uint8_t* buffer, int size) override;
        int64_t seek(int64_t offset, int whence) override;

        bool readable() const override;
        bool writable() const override;
        bool seekable() const override;

        static std::shared_ptr<generic_cursor> create(read_context&& rfunc = nullptr,
                                                      write_context&& wfunc = nullptr,
                                                      seek_context&& sfunc = nullptr);
    };

    struct random_access_cursor final : io_base, cursor
    {
        explicit random_access_cursor(const detail::multi_buffer& buffer);
        ~random_access_cursor() override = default;

        int read(uint8_t* buffer, int expect_size) override;
        int write(uint8_t* buffer, int size) override;
        int64_t seek(int64_t seek_offset, int whence) override;

        bool readable() const override;
        bool seekable() const override;

        static std::shared_ptr<random_access_cursor> create(const detail::multi_buffer& buffer);
    };
}
