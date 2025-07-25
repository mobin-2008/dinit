#ifndef DINIT_UTIL_H_INCLUDED
#define DINIT_UTIL_H_INCLUDED 1

#include <string>
#include <functional>
#include <list>
#include <vector>
#include <algorithm>
#include <stdexcept>

#include <cstring>
#include <cstddef>
#include <cerrno>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <baseproc-sys.h>
#include <mconfig.h>

#if SUPPORT_CAPABILITIES
#include <sys/capability.h>
#endif

// Check if a value is one of several possible values.
// Use like:     value(x).is_in(1,2,3)
template <typename T>
class value_cls {
    const T &v;
public:
    value_cls(const T &v) : v(v) {}

    template <typename U>
    bool is_in(U&& val)
    {
        return v == val;
    }

    template <typename U, typename ...V>
    bool is_in(U&&val, V&&... vals) {
        if (v == val)
            return true;
        return is_in(vals...);
    }
};

template <typename T>
value_cls<T> value(const T &v)
{
    return value_cls<T>(v);
}

// Since we are C++11, we don't have a std::string_view.
class string_view
{
    const char *s = nullptr;
    size_t count = 0;

public:
    string_view() = default;
    string_view(const string_view &other) = default;
    string_view(const char *s_p, size_t count_p) noexcept : s(s_p), count(count_p) { }
    string_view(const char *s_p) noexcept  : s(s_p), count(strlen(s_p)) { }

    string_view(const std::string &str) : s(str.data()), count(str.length()) { }

    string_view &operator=(const string_view &other) = default;

    bool operator==(const string_view &other) const noexcept
    {
        if (count != other.count) return false;
        return memcmp(s, other.s, count) == 0;
    }

    bool operator==(const char *other) const noexcept
    {
        // Note: we can't use strncmp, it might deduce equal strings if other is shorter and our
        // string contains a nul character.
        const char *sp = s;
        const char *op = other;
        while (*op != '\0') {
            if (*op != *sp) {
                return false;
            }
            ++op; ++sp;
        }

        return ((size_t)(op - other) == count);
    }

    const char *data() const noexcept { return s; }
    size_t size() const noexcept { return count; }
    size_t length() const noexcept { return count; }
    bool empty() const noexcept { return count == 0; }
    string_view substr(size_t begin, size_t len) const noexcept
    {
        return string_view(s + begin, std::min(len, count - begin));
    }

    const char *begin() const noexcept { return s; }
    const char *end() const noexcept { return s + count; }
    std::reverse_iterator<const char *> rbegin() const noexcept { return std::reverse_iterator<const char *>(end()); }
    std::reverse_iterator<const char *> rend() const noexcept { return std::reverse_iterator<const char *>(begin()); }

    size_t find(char c) const noexcept {
        auto it = std::find(begin(), end(), c);
        if (it == end()) return std::string::npos;
        return (it - begin());
    }
    size_t rfind(char c) const noexcept {
        auto it = std::find(rbegin(), rend(), c);
        if (it == rend()) return std::string::npos;
        return (&(*it) - begin());
    }

    char operator[](unsigned index) const noexcept { return s[index]; }

    operator std::string() const
    {
        return std::string(s, count);
    }
};

inline std::string operator+(const std::string &a, const string_view &b)
{
    std::string r = a;
    r.append(b.begin(), b.end());
    return r;
}

#if SUPPORT_CAPABILITIES
// A thin wrapper around the cap_iab_t structure to manage ownership (supports move)
struct cap_iab_wrapper
{
    cap_iab_wrapper() {}
    cap_iab_wrapper(std::string const &str) noexcept {
        if (str.empty()) return;
        // this may end up being nullptr
        // throwing from constructors is bad, so always check .get() afterwards
        iab = cap_iab_from_text(str.c_str());
    }

    cap_iab_wrapper(cap_iab_wrapper const &) = delete;
    cap_iab_wrapper(cap_iab_wrapper &&v) noexcept: iab(v.iab) {
        v.iab = nullptr;
    }

    cap_iab_wrapper &operator=(cap_iab_wrapper const &) = delete;
    cap_iab_wrapper &operator=(cap_iab_wrapper &&v) noexcept {
        iab = v.iab;
        v.iab = nullptr;
        return *this;
    }

    ~cap_iab_wrapper() noexcept {
        if (iab) cap_free(iab);
    }

    cap_iab_t get() const noexcept {
        return iab;
    }

private:
    cap_iab_t iab = nullptr;
};
#endif

// An owning wrapper around a file descriptor, ensuring that the descriptor is closed when the
// wrapper is destroyed.
class fd_holder
{
    int fd = -1;

    public:
    fd_holder() {}
    fd_holder(int fd_v) : fd(fd_v) {}
    fd_holder(fd_holder &&other) {
        fd = other.fd;
        other.fd = -1;
    }

    fd_holder &operator=(int fd_v)
    {
        if (fd != -1) bp_sys::close(fd);
        fd = fd_v;
        return *this;
    }

    fd_holder(const fd_holder &) = delete;
    fd_holder &operator=(const fd_holder &) = delete;

    ~fd_holder()
    {
        if (fd != -1) bp_sys::close(fd);
    }

    int release()
    {
        int r = fd;
        fd = -1;
        return r;
    }
};

// Complete read - read the specified size until end-of-file or error; continue read if
// interrupted by signal.
inline ssize_t complete_read(int fd, void * buf, size_t n)
{
    char * cbuf = static_cast<char *>(buf);
    ssize_t r = 0;
    while ((size_t)r < n) {
        ssize_t res = bp_sys::read(fd, cbuf + r, n - r);
        if (res == 0) {
            return r;
        }
        if (res < 0) {
            if (errno == EINTR) {
                continue;
            }

            // If any other error, and we have successfully read some, return it:
            if (r == 0) {
                return -1;
            }
            else {
                return r;
            }
        }
        r += res;
    }
    return n;
}

// Combine two paths to produce a path. If the second path is absolute, it is returned unmodified;
// otherwise, it is appended to the first path (with a slash separator added if needed).
inline std::string combine_paths(string_view p1, const char *p2)
{
    if (*p2 == 0) return (std::string)p1;
    if (p1.empty()) return std::string(p2);

    if (p2[0] == '/') return p2;

    if (*(p1.rbegin()) == '/') return (std::string)p1 + p2;
    return (std::string)p1 + '/' + p2;
}

inline std::string combine_paths(string_view p1, string_view p2)
{
    if (p2.empty()) return (std::string)p1;
    if (p1.empty()) return (std::string)p2;

    if (p2[0] == '/') return p2;

    if (*(p1.rbegin()) == '/') return (std::string)p1 + p2;
    return (std::string)p1 + '/' + p2;
}

// Find the parent path of a given path, which should refer to a named file or directory (not . or ..).
// If the path contains no directory, returns the empty string. If the path is in the root then return
// "/". Otherwise return the original path with the trailing basename removed, and all trailing slashes
// then removed.
//
// Examples:   "/a"        ->    "/"
//             "/a/b/c"    ->    "/a/b"
//             "foo"       ->    ""
//             "foo/bar"   ->    "foo"
//
// Note: for a path that contains symbolic links, the returned parent path is not the "real path".
inline string_view parent_path(string_view p)
{
    auto spos = p.rfind('/');
    if (spos == std::string::npos) {
        return string_view {};
    }

    // In case there are multiple slashes, remove the duplicates:
    while (spos > 0 && p[spos - 1] == '/') --spos;

    if (spos == 0) {
        // special case (parent is root directory)
        return {p.data(), 1};
    }

    return p.substr(0, spos);
}

// Find the base name of a path (the name after the final '/').
inline const char *base_name(const char *path) noexcept
{
    const char * basen = path;
    const char * s = path;
    while (*s != 0) {
        if (*s == '/') basen = s + 1;
        s++;
    }
    return basen;
}

// Read a symbolic link into a vector<char> (i.e. a string wrapped as a vector).
// The string will have a terminating nul character.
// Returns:
//   A pair of {success, link string}; success is false on failure (errno set). On out-of-memory,
//   the returned error is ENOMEM.
//
// Note: from C++17, this can be changed to return a string instead. Prior to that it is not
// legal to write to a string via the data pointer.
inline std::pair<bool, std::vector<char>> readlink_at_str(int dirfd, const char *path) noexcept
{
    struct stat sb;
    if (bp_sys::fstatat(dirfd, path, &sb, AT_SYMLINK_NOFOLLOW) == -1) {
        return {false, {}};
    }

    size_t vec_size = sb.st_size + 1; // add one for nul terminator

    try {
        std::vector<char> vec_str(vec_size);

        try_read_link:

        ssize_t r = bp_sys::readlinkat(dirfd, path, vec_str.data(), vec_size);

        if (r < 0) {
            return {false, {}};
        }

        if ((size_t)r > vec_size) {
            // Our size estimate was off (link changed?), try again
            vec_size = r;
            vec_str.resize(vec_size);
            goto try_read_link;
        }

        // shrink, or even expand (append nul) as necessary
        vec_str.resize(r + 1);

        return {true, std::move(vec_str)};
    }
    catch (std::bad_alloc &) {
        errno = ENOMEM;
        return {false, {}};
    }
}

// Open a file and its (real) parent directory, at the same time. This is guaranteed to work even
// if the path specifies a symbolic link (in this case, the parent directory is the parent of the
// destination, not of the link). This function is designed to avoid races that can arise when
// symlinks are present (eg even if you realpath a symlink, by the time you open that path any of
// its components may have been replaced with another symlink; we avoid that by resolving symlinks
// manually and opening the parent directory before the final path component).
// Returns:
//   A pair of file descriptors {parent dir fd, file fd}; on failure, {-1, errno}.
inline std::pair<int,int> open_with_dir(const char *dirname, const char *basename,
        int resolve_fd = AT_FDCWD) noexcept
{
    // For a file /x/y/z, if the final component 'z' is a link to /a/b/c, then the real parent
    // directory is /a/b (not /x/y).

    // This vector is used as backing for the next path if we need to "recurse" (i.e. if we need
    // to manually follow a symlink).
    std::vector<char> path_vec;

    // Whether to close resolve_fd after use (initially false because it is supplied by caller,
    // and we don't want to close the caller's fd):
    bool close_resolve_fd = false;

    long link_resolve_times = sysconf(_SC_SYMLOOP_MAX);

    // strip the basename and open as a directory
    #ifdef O_SEARCH
        constexpr int dir_search_flag = O_SEARCH;
    #else
        // O_SEARCH is not available on Linux
        constexpr int dir_search_flag = O_PATH;
    #endif

    begin:

    if (*dirname == '\0') {
        dirname = ".";
    }

    int parent_dir_fd = bp_sys::openat(resolve_fd, dirname, dir_search_flag | O_DIRECTORY);
    if (close_resolve_fd) bp_sys::close(resolve_fd);
    if (parent_dir_fd == -1) {
        return {-1, errno};
    }

    // open the final file (basename) without following symlinks
    int file_fd = bp_sys::openat(parent_dir_fd, basename, O_RDONLY | O_NOFOLLOW);
    if (file_fd == -1) {
        if (errno == ELOOP) {
            // It's a symlink
            if (link_resolve_times == 0) {
                // We've tried to resolve as many as the system normally would, so give up.
                bp_sys::close(parent_dir_fd);
                return {-1, ELOOP};
            }

            link_resolve_times--;

            // read the next path
            auto link_path_r = readlink_at_str(parent_dir_fd, basename);
            if (!link_path_r.first) {
                // If EINVAL, it's supposedly not a symbolic link. Maybe it changed out from
                // underneath us. The risk of handling this by looping back and trying as a file
                // again seems greater than the risk that this will actually happen, so let's not
                // worry about it, and just error out now:
                bp_sys::close(parent_dir_fd);
                return {-1, errno};
            }

            // Ok, we successfully read the link target. We need to extract the parent directory
            // name and base name, and then we'll jump back to 'begin' to iterate:
            path_vec = std::move(link_path_r.second);
            char *path = path_vec.data();
            dirname = path;
            basename = base_name(dirname);
            if (basename == dirname) {
                dirname = "";
            }
            else if (*basename == '\0') {
                bp_sys::close(parent_dir_fd);
                return {-1, ENOENT};
            }
            else {
                string_view path_ish {path, (size_t)(basename - path)};
                string_view parent_path_v = parent_path(path_ish);
                if (parent_path_v.length() == 1) {
                    // Parent path must be '/'. Inserting nul might overwrite base name string,
                    // so we need a separate string:
                    dirname = "/";
                }
                else if (parent_path_v.empty()) {
                    dirname = "";
                }
                else {
                    path[parent_path_v.length()] = '\0';
                }
            }

            resolve_fd = parent_dir_fd;
            close_resolve_fd = true;
            goto begin;
        }

        // Other errors: bail out
        bp_sys::close(parent_dir_fd);
        return {-1, errno};
    }

    return {parent_dir_fd, file_fd};
}

// Check if one string starts with another
inline bool starts_with(const std::string &s, const char *prefix) noexcept
{
    const char *sp = s.c_str();
    while (*sp != 0 && *prefix != 0) {
        if (*sp != *prefix) return false;
        sp++; prefix++;
    }
    return *prefix == 0;
}

// An allocator that doesn't value-initialise for construction. Eg for containers of primitive types this
// allocator avoids the overhead of initialising new elements to 0.
template <typename T>
class default_init_allocator : public std::allocator<T>
{
    using std::allocator<T>::allocator;

public:
    template <typename U>
    struct rebind {
        using other = default_init_allocator<U>;
    };

    template <typename U = std::enable_if<std::is_default_constructible<T>::value>>
    void construct(T *obj)
    {
        // avoid value-initialisation:
        ::new(obj) T;
    }

    template <typename ...Args>
    void construct(T *obj, Args&&... args)
    {
        std::allocator<T>::construct(obj, std::forward<Args>(args)...);
    }
};

inline size_t hash(const string_view &str)
{
    size_t end_pos = str.length();
    size_t hash_val = 0;
    for (size_t i = 0; i < end_pos; i += sizeof(size_t)) {
        // collect as many characters as will fit into size_t
        size_t hash_unit = 0;
        std::memcpy(&hash_unit, str.data() + i, std::min(sizeof(size_t), end_pos - i));
        // then incorporate the collected characters into the hash value
        hash_val *= 31;
        hash_val += hash_unit;
    }
    return hash_val;
}

struct hash_sv
{
    size_t operator()(const string_view &str) const
    {
        return hash(str);
    }
};

inline bool operator==(string_view str1, const std::string &str2)
{
    return str1 == string_view(str2);
}

inline bool operator==(const std::string &str2, string_view str1)
{
    return str1 == string_view(str2);
}

// An equivalent to std::equal_to<void> (which is C++14)
class dinit_equal_to
{
public:
    template <typename A, typename B>
    bool operator()(A &&a, B &&b)
    {
        return std::forward<A>(a) == std::forward<B>(b);
    }
};

// A set where we can check for membership via other-than-key-type values
template <typename K, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>>
class dinit_unordered_set {

    using key_type = K;
    using value_type = K;
    using size_type = size_t;
    using hasher = Hash;
    using key_equal = Equal;

    hasher hash_f;
    key_equal key_equal_f;

    using bucket_vec = std::vector<std::list<K>>;
    bucket_vec buckets {};

    size_t current_size = 0;
    size_t current_limit = 0; // size limit before we need more buckets

    size_t buckets_to_max(size_t buckets) noexcept
    {
        // calculate "buckets * 3 / 4" but without risk of overflow at the multiply stage
        size_t base = buckets / 4 * 3;
        size_t extra = (buckets % 4) * 3 / 4;
        return base + extra;
    }

    void do_rehash(size_t new_buckets) noexcept
    {
        // note, buckets vector is already at least new_buckets in size
        size_t old_buckets = buckets.size();

        // first splice all nodes from all buckets into a single list
        std::list<K> all_nodes;
        for (size_t i = 0; i < old_buckets; ++i) {
            all_nodes.splice(all_nodes.end(), buckets[i]);
        }

        // now put all nodes into the correct bucket
        auto node_i = all_nodes.begin();
        while (node_i != all_nodes.end()) {
            auto next_node_i = std::next(node_i);
            size_t hashval = hash_f(*node_i);
            size_t bucket_num = hashval % new_buckets;
            buckets[bucket_num].splice(buckets[bucket_num].end(), all_nodes, node_i);
            node_i = next_node_i;
        }
    }

public:
    class iterator
    {
        friend class dinit_unordered_set;

    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = dinit_unordered_set::value_type;
        using pointer = value_type*;
        using reference = value_type&;

    protected:
        using list_iterator_t = typename std::list<K>::iterator;

        bucket_vec *buckets;
        list_iterator_t list_it;
        size_t bucket_num;

    public:
        iterator() noexcept : buckets(nullptr), list_it(), bucket_num(-1) { }
        iterator(bucket_vec *buckets_p, list_iterator_t lit, size_t bucket_p) noexcept
            : buckets(buckets_p), list_it(lit), bucket_num(bucket_p) { }
        iterator(const iterator &) noexcept = default;

        bool operator==(const iterator &other) noexcept
        {
            return other.bucket_num == bucket_num && other.list_it == list_it;
        }

        bool operator!=(const iterator &other) noexcept
        {
            return !(*this == other);
        }

        value_type &operator*() noexcept
        {
            return *list_it;
        }

        const value_type *operator->() noexcept
        {
            return &(*list_it);
        }

        iterator &operator++() noexcept
        {
            if (++list_it == (*buckets)[bucket_num].end()) {
                for (size_type i = bucket_num + 1; i < buckets->size(); ++i) {
                    if (!(*buckets)[i].empty()) {
                        list_it = (*buckets)[i].begin();
                        bucket_num = i;
                        return *this;
                    }
                }
                list_it = {};
                bucket_num = -1;
            }
            return *this;
        }
    };

    class const_iterator : public iterator
    {
    public:
        using iterator::iterator;

        using pointer = const typename iterator::value_type*;
        using reference = const typename iterator::value_type&;

        const_iterator(const const_iterator &) noexcept = default;
        const_iterator(const iterator &other) noexcept : iterator(other) { }

        const value_type &operator*() noexcept
        {
            return *iterator::list_it;
        }

        const value_type *operator->() noexcept
        {
            return &(*iterator::list_it);
        }
    };

private:
    // implement insert as a template so we can use it for rvalues and lvalues alike
    template <typename V>
    std::pair<iterator,bool> do_insert(V &&value)
    {
        size_t hashval = hash_f(value);
        size_t bucket_num;

        if (buckets.empty()) {
            buckets.resize(4); // good enough starting point
            current_limit = 3;
            bucket_num = hashval % buckets.size();
        }
        else {
            // First, check if the value is already present
            bucket_num = hashval % buckets.size();
            auto list_it = std::find_if(buckets[bucket_num].begin(), buckets[bucket_num].end(),
                    [&](const key_type &k) { return key_equal_f(k,std::forward<V>(value)); });
            if (list_it != buckets[bucket_num].end()) {
                return { { &buckets, list_it, bucket_num }, false };
            }

            // Not present. Check if we need to expand.
            if (current_size >= current_limit) {
                if (buckets.size() <= (buckets.max_size() / 2)) {
                    rehash(buckets.size() * 2);
                    current_limit *= 2;
                    bucket_num = hashval % buckets.size();
                }
                else {
                    // in the unlikely event that we have hit the max_size, let's just become overloaded
                    current_limit = -1;
                }
            }
        }

        auto list_it = buckets[bucket_num].insert(buckets[bucket_num].end(), std::forward<V>(value));
        ++current_size;
        return { { &buckets, list_it, bucket_num }, true };
    }

public:
    dinit_unordered_set() noexcept = default;

    iterator end() noexcept
    {
        return iterator();
    }

    const_iterator end() const noexcept
    {
        return const_iterator();
    }

    iterator begin() noexcept
    {
        if (current_size == 0) return end();
        for (size_t i = 0; i < buckets.size(); ++i) {
            if (!buckets[i].empty()) {
                return { &buckets, buckets[i].begin(), i };
            }
        }
        return end(); // (should not be reachable)
    }

    const_iterator begin() const noexcept
    {
        auto *non_const_this = const_cast<dinit_unordered_set *>(this);
        return non_const_this->begin();
    }

    std::pair<iterator,bool> insert(const value_type &value)
    {
        return do_insert(value);
    }

    std::pair<iterator,bool> insert(value_type &&value)
    {
        return do_insert(std::move(value));
    }

    // Not available in C++: insert a different type of value; assumes that hash and equals
    // predicate can deal with the argument type, and that it can be converted to the actual
    // value.
    template <typename V>
    std::pair<iterator,bool> insert_byval(V &&value)
    {
        return do_insert(std::forward<V>(value));
    }

    void rehash(size_type new_bucket_count)
    {
        // calculate minimum bucket count, limited by maximum possible count.
        size_t max_buckets = buckets.max_size();
        size_t max_count = buckets_to_max(max_buckets);
        size_t min_buckets;
        if (current_size > max_count) {
            min_buckets = max_buckets;
        }
        else {
            size_t base = current_size / 3 * 4;
            size_t extra = (current_size % 3) * 4 / 3;
            min_buckets = base + extra;
        }

        new_bucket_count = std::max(new_bucket_count, min_buckets);

        if (new_bucket_count < buckets.size()) {
            // buckets vector will shrink: we need to reallocate entries into their new buckets first
            do_rehash(new_bucket_count);
            buckets.resize(new_bucket_count);
        }
        else {
            // grow the vector, then rehash
            buckets.resize(new_bucket_count);
            do_rehash(new_bucket_count);
        }
    }

    template <typename V>
    iterator find(const V &value) noexcept
    {
        if (buckets.empty()) return end();
        size_t hashval = hash_f(value);
        size_t bucket_num = hashval % buckets.size();
        for (auto list_it = buckets[bucket_num].begin(); list_it != buckets[bucket_num].end(); ++list_it) {
            if (key_equal_f(*list_it, value)) {
                return { &buckets, list_it, bucket_num };
            }
        }
        return end();
    }

    template <typename V>
    const_iterator find(const V &value) const noexcept
    {
        auto *non_const_this = const_cast<dinit_unordered_set *>(this);
        return non_const_this->find(value);
    }

    iterator erase(iterator pos) noexcept
    {
        auto new_it = std::next(pos);
        buckets[pos.bucket_num].erase(pos.list_it);
        return new_it;
    }

    template <typename V>
    size_type erase(const V &value) noexcept
    {
        auto it = find(value);
        if (it != end()) {
            buckets[it.bucket_num].erase(it.list_it);
            return 1;
        }
        return 0;
    }

    template <typename V>
    bool contains(const V &value) const noexcept
    {
        return find(value) != end();
    }

    size_type size() const noexcept
    {
        return current_size;
    }

    bool empty() const noexcept
    {
        return current_size == 0;
    }

    void clear() noexcept
    {
        for (auto &bucket : buckets) {
            bucket.clear();
        }
        current_size = 0;
        current_limit = 0;
        buckets.clear();
        try {
            buckets.shrink_to_fit();
        }
        catch (std::bad_alloc &) {
            // ignore any (unlikely) error
        }
    }
};

// a linked-list-set, i.e. a set that tracks insertion order
template <typename K, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>>
class linked_uo_set
{
    using key_type = K;
    using value_type = K;
    using size_type = size_t;
    using hasher = Hash;
    using key_equal = Equal;

    struct linked_record
    {
        K value;
        linked_record *next;
    };

    struct lr_hash
    {
        hasher lr_hash_f;

        template <typename T>
        size_t operator()(const T &hval)
        {
            return lr_hash_f(hval);
        }

        size_t operator()(const linked_record &r)
        {
            return lr_hash_f(r.value);
        }
    };

    struct lr_equ
    {
        key_equal lr_equal_f;

        template <typename T>
        bool operator()(const T &hval, const linked_record &rec)
        {
            return lr_equal_f(hval, rec.value);
        }

        template <typename T>
        bool operator()(const linked_record &rec, const T &hval)
        {
            return lr_equal_f(hval, rec.value);
        }

        bool operator()(const linked_record &rec1, const linked_record &rec2)
        {
            return lr_equal_f(rec1.value, rec2.value);
        }
    };

    dinit_unordered_set<linked_record, lr_hash, lr_equ> backing;

    linked_record *first = nullptr;
    linked_record *last = nullptr;

public:
    // Add to the back of the linked set, if not already in the set.
    // Return true if added, false if was already in the set.
    bool add_back(const value_type &value)
    {
        auto it_b = backing.insert({value, nullptr});
        if (it_b.second) {
            auto *new_ent = &(*it_b.first);
            if (last != nullptr) {
                last->next = new_ent;
            }
            last = new_ent;
            if (first == nullptr) {
                first = new_ent;
            }
            return true;
        }
        return false;
    }

    class iterator
    {
        linked_record *current = nullptr;

    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = linked_uo_set::value_type;
        using pointer = value_type*;
        using reference = value_type&;

        iterator(linked_record *record) : current(record) { }

        bool operator==(const iterator &other) noexcept
        {
            return other.current == current;
        }

        bool operator!=(const iterator &other) noexcept
        {
            return !(*this == other);
        }

        value_type &operator*() noexcept
        {
            return current->value;
        }

        const value_type *operator->() noexcept
        {
            return &(current->value);
        }

        iterator &operator++() noexcept
        {
            current = current->next;
            return *this;
        }
    };

    iterator begin()
    {
        return iterator(first);
    }

    iterator end()
    {
        return iterator(nullptr);
    }
};

// string that maintains a heap allocation always. Moving a ha_string does not invalidate pointers
// to characters within the string.
class ha_string {
    char *str_data = nullptr;
    size_t str_len = 0;

public:
    ha_string()
    {
    }

    ha_string(const char *cstr, size_t clen)
    {
        str_data = new char[clen + 1];
        memcpy(str_data, cstr, clen + 1);
        str_len = clen;
    }

    ha_string(const char *cstr) : ha_string(cstr, strlen(cstr))
    {
    }

    ha_string(const ha_string &other)
    {
        operator=(other);
    }

    ha_string(const ha_string &&other)
    {
        operator=(std::move(other));
    }

    ha_string &operator=(const ha_string &other)
    {
        char *new_str_data = new char[other.str_len + 1];
        delete[] str_data;
        memcpy(new_str_data, other.str_data, other.str_len + 1);
        str_data = new_str_data;
        str_len = other.str_len;
        return *this;
    }

    ha_string &operator=(ha_string &&other) noexcept
    {
        delete[] str_data;
        str_data = other.str_data;
        str_len = other.str_len;
        other.str_data = nullptr;
        other.str_len = 0;
        return *this;
    }

    ha_string &operator=(const std::string &other)
    {
        char *new_str_data = new char[other.length() + 1];
        delete[] str_data;
        memcpy(new_str_data, other.data(), other.length() + 1);
        str_data = new_str_data;
        str_len = other.length();
        return *this;
    }

    ~ha_string()
    {
        delete[] str_data;
    }

    char &operator[](size_t index) noexcept
    {
        return str_data[index];
    }

    bool operator==(const char *other) noexcept
    {
        return strncmp(str_data, other, str_len) == 0;
    }

    char *c_str() noexcept
    {
        return str_data;
    }

    const char *c_str() const noexcept
    {
        return str_data;
    }

    bool empty() const noexcept
    {
        return str_len == 0;
    }

    size_t length() const noexcept
    {
        return str_len;
    }

    std::string substr(size_t pos, size_t count = (size_t)-1)
    {
        if (pos > str_len) throw std::out_of_range("pos exceeds string length");
        size_t sub_len = std::min(count, str_len - pos);
        return std::string(str_data + pos, sub_len);
    }
};

#endif
