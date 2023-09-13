#include <limits>
#include <cstdio>
#include <cstring>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <dinit-iostream.h>
#include <baseproc-sys.h>

// See dinit-iostream.h for inforamtion about the usage

namespace {

// A simple trick for calculating how much digits can be in a type with compile-time integer
// logarithm like function.
template <typename T> constexpr unsigned type_max_numdigits(T num = std::numeric_limits<T>::max(),
        unsigned pow = 0)
{
    return (num == 0) ? pow : type_max_numdigits(num / 10, pow + 1);
}

// We should have space for sign in signed variables.
constexpr unsigned UINT_MAX_CHARS = type_max_numdigits<unsigned>();
constexpr unsigned INT_MAX_CHARS = type_max_numdigits<int>() + 1;
constexpr unsigned ULONG_MAX_CHARS = type_max_numdigits<unsigned long>();
constexpr unsigned LONG_MAX_CHARS = type_max_numdigits<long>() + 1;

} /* anonymous namespace */

namespace dio {

// class io_base

streambuf *io_base::get_buf() noexcept
{
    return buf.get();
}

bool io_base::is_open() noexcept
{
    return (fd >= 0);
}

void io_base::set_fd(const int newfd) noexcept
{
    fd = newfd;
}

int io_base::get_fd() noexcept
{
    return fd;
}

// class ostream

ssize_t ostream::put(const char *msg, size_t count) noexcept
{
    if (!good()) {
        return -1;
    }

    if (count == 0) {
        return 0;
    }

    size_t remaining_bytes = count;
    while (remaining_bytes > buf->get_free()) {
        struct iovec bufv[3];
        char *ptr = buf->get_ptr(0);

        bufv[0].iov_base = ptr;
        bufv[0].iov_len = buf->get_contiguous_length(ptr);
        if (buf->get_length() != bufv[0].iov_len) {
            // buffer's data in streambuf is splited into two parts in this case because of cpbuffer nature.
            bufv[1].iov_base = buf->get_buf_base();
            bufv[1].iov_len = buf->get_length() - bufv[0].iov_len;
        }
        else {
            bufv[1].iov_base = nullptr;
            bufv[1].iov_len = 0;
        }
        bufv[2].iov_base = const_cast<char *>(msg);
        bufv[2].iov_len = remaining_bytes;

        unsigned buffer_length = bufv[0].iov_len + bufv[1].iov_len;

        ssize_t cur_written_bytes = bp_sys::writev(fd, bufv, 3);
        if (cur_written_bytes < 0) {
            io_error = errno;
            // Some bytes may be written in previous iteration
            return (count - remaining_bytes > 0) ? count - remaining_bytes : -1;
        }
        if (cur_written_bytes >= buffer_length) {
            buf->consume(buffer_length);
            cur_written_bytes -= buffer_length;
            remaining_bytes -= cur_written_bytes;
            msg += cur_written_bytes;
        }
        else {
            buf->consume(cur_written_bytes);
        }

        if (remaining_bytes == 0) {
            return count;
        }
    }

    buf->append(msg, remaining_bytes);
    return count;
}

void ostream::throw_exception_on(const int states)
{
    if ((states & io_states::buffer_failbit) && buffer_failure()) {
        throw iostream_internal_err("Cannot use stream's buffer: buffer is nullptr!");
    }
    if ((states & io_states::io_failbit) && io_failure()) {
        throw iostream_system_err(io_failure());
    }
}

bool ostream::open_nx(const char *path) noexcept
{
    fd = bp_sys::open(path, O_WRONLY);

    if (fd < 0) {
        io_error = errno;
        return false;
    }
    return true;
}

bool ostream::open(const char *path)
{
    bool r = open_nx(path);
    if (!r) {
        throw_exception_on(io_states::io_failbit);
    }
    return r;
}

bool ostream::open_nx(const char *path, const int flags) noexcept
{
    fd = bp_sys::open(path, O_WRONLY | flags);

    if (fd < 0) {
        io_error = errno;
        return false;
    }
    return true;
}

bool ostream::open(const char *path, const int flags)
{
    bool r = open_nx(path, flags);
    if (!r) {
        throw_exception_on(io_states::io_failbit);
    }
    return r;
}

bool ostream::open_nx(const char *path, const int flags, const mode_t mode) noexcept
{
    fd = bp_sys::open(path, O_WRONLY | flags, mode);

    if (fd < 0) {
        io_error = errno;
        return false;
    }
    return true;
}

bool ostream::open(const char *path, const int flags, const mode_t mode)
{
    bool r = open_nx(path, flags, mode);
    if (!r) {
        throw_exception_on(io_states::io_failbit);
    }
    return r;
}

bool ostream::close_nx() noexcept
{
    if (!flush_nx()) {
        // io_error was set by flush_nx() call
        return false;
    }
    if (bp_sys::close(fd) < 0) {
        io_error = errno;
        return false; // Failed
    }

    fd = -1;
    return true;
}

bool ostream::close()
{
    bool r = close_nx();
    if (!r) {
        // This will definitely throw an exception
        throw_exception_on(io_states::buffer_failbit | io_states::io_failbit);
    }
    return r;
}

int ostream::current_state() noexcept
{
    int bits = 0;
    if (!buf) bits |= io_states::buffer_failbit;
    if (io_error) bits |= io_states::io_failbit;
    return bits;
}

bool ostream::good() noexcept
{
    return (current_state() == 0);
}

bool ostream::buffer_failure() noexcept
{
    return (!buf);
}

int ostream::io_failure() noexcept
{
    return io_error;
}

void ostream::clear() noexcept
{
    io_error = 0;
}

bool ostream::flush_nx() noexcept
{
    if (!good()) {
        return -1;
    }

    while (buf->get_length() > 0) {
        char *ptr = buf->get_ptr(0);
        unsigned len = buf->get_contiguous_length(ptr);

        unsigned iovs_to_write = 1;
        struct iovec bufv[2];

        bufv[0].iov_base = ptr;
        bufv[0].iov_len = len;
        if (buf->get_length() != len) {
            // buffer's data in streambuf is splited into two parts in this case because of cpbuffer nature.
            iovs_to_write = 2;
            bufv[1].iov_base = buf->get_buf_base();
            bufv[1].iov_len = buf->get_length() - len;
        }

        ssize_t r = bp_sys::writev(fd, bufv, iovs_to_write);
        if (r < 0) {
            io_error = errno;
            return false;
        }
        buf->consume(r);
    }

    return true;
}

bool ostream::flush()
{
    bool r = flush_nx();
    if (!r) {
        // This will definitely throw an exception
        throw_exception_on(io_states::buffer_failbit | io_states::io_failbit);
    }
    return r;
}

bool ostream::write_nx(const char *msg) noexcept
{
    size_t len = strlen(msg);
    return (put(msg, len) == static_cast<ssize_t>(len));
}

bool ostream::write(const char *msg)
{
    bool r = write_nx(msg);
    if (!r) {
        // This will definitely throw an exception
        throw_exception_on(io_states::buffer_failbit | io_states::io_failbit);
    }
    return r;
}

bool ostream::write_nx(char msg) noexcept
{
    return (put(&msg, 1) == 1);
}

bool ostream::write(char msg)
{
    bool r = write_nx(msg);
    if (!r) {
        // This will definitely throw an exception
        throw_exception_on(io_states::buffer_failbit | io_states::io_failbit);
    }
    return r;
}

bool ostream::write_nx(const std::string &msg) noexcept
{
    return (put(msg.c_str(), msg.size()) == static_cast<ssize_t>(msg.size()));
}

bool ostream::write(const std::string &msg)
{
    bool r = write_nx(msg);
    if (!r) {
        // This will definitely throw an exception
        throw_exception_on(io_states::buffer_failbit | io_states::io_failbit);
    }
    return r;
}

bool ostream::write_nx(const int num) noexcept
{
    char tmp[INT_MAX_CHARS + 1];
    int len = snprintf(tmp, INT_MAX_CHARS + 1, "%d", num);
    return (put(tmp, len) == len);
}

bool ostream::write(const int num)
{
    bool r = write_nx(num);
    if (!r) {
        // This will definitely throw an exception
        throw_exception_on(io_states::buffer_failbit | io_states::io_failbit);
    }
    return r;
}

bool ostream::write_nx(const unsigned num) noexcept
{
    char tmp[UINT_MAX_CHARS + 1];
    int len = snprintf(tmp, UINT_MAX_CHARS + 1, "%d", num);
    return (put(tmp, len) == len);
}

bool ostream::write(const unsigned num)
{
    bool r = write_nx(num);
    if (!r) {
        // This will definitely throw an exception
        throw_exception_on(io_states::buffer_failbit | io_states::io_failbit);
    }
    return r;
}

bool ostream::write_nx(const long num) noexcept
{
    char tmp[LONG_MAX_CHARS + 1];
    int len = snprintf(tmp, LONG_MAX_CHARS + 1, "%ld", num);
    return (put(tmp, len) == len);
}

bool ostream::write(const long num)
{
    bool r = write_nx(num);
    if (!r) {
        // This will definitely throw an exception
        throw_exception_on(io_states::buffer_failbit | io_states::io_failbit);
    }
    return r;
}

bool ostream::write_nx(const unsigned long num) noexcept
{
    char tmp[ULONG_MAX_CHARS + 1];
    int len = snprintf(tmp, ULONG_MAX_CHARS + 1, "%ld", num);
    return (put(tmp, len) == len);
}

bool ostream::write(const unsigned long num)
{
    bool r = write_nx(num);
    if (!r) {
        // This will definitely throw an exception
        throw_exception_on(io_states::buffer_failbit | io_states::io_failbit);
    }
    return r;
}

bool ostream::write_nx(const endline &) noexcept
{
    int r = put("\n", 1);
    r += flush_nx();
    return (r >= 1);
}

bool ostream::write(const endline &)
{
    bool r = write_nx(endl);
    if (!r) {
        // This will definitely throw an exception
        throw_exception_on(io_states::buffer_failbit | io_states::io_failbit);
    }
    return r;
}

bool ostream::write_nx(const flushbuf &) noexcept
{
    return (flush_nx());
}

bool ostream::write(const flushbuf &)
{
    return (flush());
}

ssize_t ostream::write_buf_nx(const char *msg) noexcept
{
    return put(msg, strlen(msg));
}

ssize_t ostream::write_buf(const char *msg)
{
    ssize_t r = write_buf_nx(msg);
    if (r < 0) {
        // This will definitely throw an exception
        throw_exception_on(io_states::buffer_failbit | io_states::io_failbit);
    }
    return r;
}

ssize_t ostream::write_buf_nx(const std::string &msg) noexcept
{
    return put(msg.c_str(), msg.size());
}

ssize_t ostream::write_buf(const std::string &msg)
{
    ssize_t r = write_buf_nx(msg);
    if (r < 0) {
        // This will definitely throw an exception
        throw_exception_on(io_states::buffer_failbit | io_states::io_failbit);
    }
    return r;
}

ostream::operator bool() noexcept
{
    return good();
}

ostream::~ostream() noexcept
{
    // We don't use close()/close_nx() functions because we cannot do anything about flush_nx() failure
    // in this stage.
    if (!is_open()) return;
    flush_nx();
    bp_sys::close(get_fd());
}

// class istream

int istream::load_into_buf(unsigned len) noexcept
{
    int r = buf->fill(fd, len);
    if (r < 0) {
        io_error = errno;
        return -1;
    }
    return r;
}

void istream::throw_exception_on(const int states)
{
    if ((states & io_states::eofbit) && eof()) {
        throw iostream_eof("End Of File has been reached.");
    }
    if ((states & io_states::buffer_failbit) && buffer_failure()) {
        throw iostream_internal_err("Cannot use stream's buffer: buffer is nullptr!");
    }
    if ((states & io_states::string_failbit) && string_failure()) {
        throw iostream_internal_err("Cannot put lines into given std::string.");
    }
    if ((states & io_states::io_failbit) && io_failure()) {
        throw iostream_system_err(io_failure());
    }
}

bool istream::open_nx(const char *path) noexcept
{
    fd = bp_sys::open(path, O_RDONLY);

    if (fd < 0) {
        io_error = errno;
        return false;
    }
    return true;
}

bool istream::open(const char *path)
{
    bool r = open_nx(path);
    if (!r) {
        throw_exception_on(io_states::io_failbit);
    }
    return r;
}

bool istream::open_nx(const char *path, const int flags) noexcept
{
    fd = bp_sys::open(path, O_RDONLY | flags);

    if (fd < 0) {
        io_error = errno;
        return false;
    }
    return true;
}

bool istream::open(const char *path, const int flags)
{
    bool r = open_nx(path, flags);
    if (!r) {
        throw_exception_on(io_states::io_failbit);
    }
    return r;
}

bool istream::open_nx(const char *path, const int flags, const mode_t mode) noexcept
{
    fd = bp_sys::open(path, O_RDONLY | flags, mode);

    if (fd < 0) {
        io_error = errno;
        return false;
    }
    return true;
}

bool istream::open(const char *path, const int flags, const mode_t mode)
{
    bool r = open_nx(path, flags, mode);
    if (!r) {
        throw_exception_on(io_states::io_failbit);
    }
    return r;
}

bool istream::close_nx() noexcept
{
    if (bp_sys::close(fd) < 0) {
        io_error = errno;
        return false; // Failed
    }

    fd = -1;
    return true;
}

bool istream::close()
{
    bool r = close_nx();
    if (!r) {
        throw_exception_on(io_states::io_failbit);
    }
    return r;
}

int istream::current_state() noexcept
{
    int bits = 0;
    if (eof_state) bits |= io_states::eofbit;
    if (!buf) bits |= io_states::buffer_failbit;
    if (string_failed) bits |= io_states::string_failbit;
    if (io_error) bits |= io_states::io_failbit;
    return bits;
}

bool istream::good() noexcept
{
    return (current_state() == 0);
}

bool istream::eof() noexcept
{
    return eof_state;
}

bool istream::buffer_failure() noexcept
{
    return (!buf);
}

bool istream::string_failure() noexcept
{
    return string_failed;
}

int istream::io_failure() noexcept
{
    return io_error;
}

void istream::clear() noexcept
{
    eof_state = false;
    string_failed = false;
    io_error = 0;
}

int istream::getc_nx() noexcept
{
    if (eof() || buffer_failure() || io_failure()) {
        // We don't care about string_failure() in getc() function.
        return -1;
    }
    if (buf->get_length() == 0) {
        int r = load_into_buf(1);
        if (r == 0) {
            eof_state = true;
            return -1;
        }
        if (r < 0) {
            io_error = errno;
            return -1;
        }
    }
    char result = *buf->get_ptr(0);
    buf->consume(1);
    return result;
}

int istream::getc()
{
    int r = getc_nx();
    if (r < 0) {
        throw_exception_on(io_states::eofbit | io_states::buffer_failbit | io_states::io_failbit);
    }
    return r;
}

bool istream::get_line_nx(std::string &dest, char delim) noexcept
{
    if (!good()) {
        return false;
    }
    if (buf->get_length() == 0) {
        int r = load_into_buf(buf->get_size());
        if (r == 0) {
            eof_state = true;
            return false;
        }
        if (r < 0) {
            io_error = errno;
            return false;
        }
    }
    dest.clear();

    char *ptr = buf->get_ptr(0);
    unsigned len = buf->get_contiguous_length(ptr);
    char *eptr = static_cast<char *>(std::memchr(ptr, delim, len));

    if (eptr == ptr) {
        buf->consume(1);
        return true; // An empty line
    }
    while (eptr == nullptr) {
        // If it hasn't any delim; We try to read as much as possible to reach the EOF or find a
        // "delim" character.
        try {
            // dest.append() may throw out_of_range, length_error or bad_alloc
            dest.append(ptr, len);
        }
        catch (const std::exception &e) {
            string_failed = true;
            return false;
        }
        buf->consume(len);

        if (buf->get_length() == 0) {
            int r = load_into_buf(buf->get_size());
            if (r == 0) {
                return true;
            }
            if (r < 0) {
                io_error = errno;
                return false;
            }
        }

        ptr = buf->get_ptr(0);
        len = buf->get_contiguous_length(ptr);
        eptr = static_cast<char *>(std::memchr(ptr, delim, len));
    }
    unsigned index = eptr - ptr;
    try {
        // dest.append() may throw out_of_range, length_error or bad_alloc
        dest.append(ptr, index);
    }
    catch (const std::exception &e) {
        string_failed = true;
        return false;
    }
    buf->consume(index + 1); // Consume the delimiter character as well
    return true;
}

bool istream::get_line(std::string &dest, char delim)
{
    bool r = get_line_nx(dest, delim);
    if (!r) {
        // This will definitely throw an exception
        throw_exception_on(io_states::eofbit | io_states::buffer_failbit | io_states::string_failbit
                | io_states::io_failbit);
    }
    return r;
}

bool istream::get_line_until_eof(std::string &dest, char delim)
{
    bool r = get_line_nx(dest, delim);
    if (!r) {
        throw_exception_on(io_states::buffer_failbit | io_states::string_failbit | io_states::io_failbit);
    }
    return r;
}

istream::operator bool() noexcept
{
    return good();
}

istream::~istream() noexcept
{
    if (!is_open()) return;
    close_nx();
}

} /* dio namespace */
