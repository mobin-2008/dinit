#ifndef CPBUFFER_H
#define CPBUFFER_H

#include <cstring>

// control protocol buffer, a circular buffer with 1024-byte capacity.
template <int SIZE> class CPBuffer
{
    char buf[SIZE];
    int cur_idx = 0;
    int length = 0;  // number of elements in the buffer
    
    public:
    int get_length() noexcept
    {
        return length;
    }
    
    int get_free() noexcept
    {
        return SIZE - length;
    }
    
    char * get_ptr(int index)
    {
        int pos = cur_idx + index;
        if (pos >= SIZE) pos -= SIZE;
    
        return &buf[cur_idx];
    }
    
    int get_contiguous_length(char *ptr)
    {
        int eidx = cur_idx + length;
        if (eidx >= SIZE) eidx -= SIZE;
        
        if (buf + eidx > ptr) {
            return (buf + eidx) - ptr;
        }
        else {
            return (buf + SIZE) - ptr;
        }
    }
    
    // fill by reading from the given fd, return positive if some was read or -1 on error.
    int fill(int fd) noexcept
    {
        int pos = cur_idx + length;
        if (pos >= SIZE) pos -= SIZE;
        int max_count = std::min(SIZE - pos, SIZE - length);
        ssize_t r = read(fd, buf + pos, max_count);
        if (r >= 0) {
            length += r;
        }
        return r;
    }
    
    // fill by readin from the given fd, until at least the specified number of bytes are in
    // the buffer. Return 0 if end-of-file reached before fill complete, or -1 on error.
    int fillTo(int fd, int rlength) noexcept
    {
        while (length < rlength) {
            int r = fill(fd);
            if (r <= 0) return r;
        }
        return 1;
    }
    
    char operator[](int idx) noexcept
    {
        int dest_idx = cur_idx + idx;
        if (dest_idx > SIZE) dest_idx -= SIZE;
        return buf[dest_idx];
    }
    
    // Remove the given number of bytes from the start of the buffer.
    void consume(int amount) noexcept
    {
        cur_idx += amount;
        if (cur_idx >= SIZE) cur_idx -= SIZE;
        length -= amount;
    }
    
    // Extract bytes from the buffer. The bytes remain in the buffer.
    void extract(char *dest, int index, int length) noexcept
    {
        index += cur_idx;
        if (index >= SIZE) index -= SIZE;
        if (index + length > SIZE) {
            // wrap-around copy
            int half = SIZE - index;
            std::memcpy(dest, buf + index, half);
            std::memcpy(dest + half, buf, length - half);
        }
        else {
            std::memcpy(dest, buf + index, length);
        }
    }
    
    // Extract string of given length from given index
    // Throws:  std::bad_alloc on allocation failure
    std::string extract_string(int index, int length)
    {
        index += cur_idx;
        if (index >= SIZE) index -= SIZE;
        if (index + length > SIZE) {
            std::string r(buf + index, SIZE - index);
            r.insert(r.end(), buf, buf + length - (SIZE - index));
            return r;
        }
        else {
            return std::string(buf + index, length);
        }
    }
    
    // Append characters to the buffer. Caller must make certain there
    // is enough space to contain the characters first.
    void append(const char * s, int len) noexcept
    {
        int index = cur_idx + length;
        if (index >= SIZE) index -= SIZE;

        length += len; // (before we destroy len)
        
        int max = SIZE - index;
        std::memcpy(buf + index, s, std::min(max, len));
        if (len > max) {
            // Wrapped around buffer: copy the rest
            s += max;
            len -= max;
            std::memcpy(buf, s, len);
        }
    }
};

#endif