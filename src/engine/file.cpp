#include "scratchbird/engine/file.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

namespace scratchbird::engine
{

    static inline void throw_io(const char* what)
    {
        throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
    }

    FileHandle::~FileHandle()
    {
        if (fd_ >= 0)
            ::close(fd_);
    }

    FileHandle::FileHandle(FileHandle&& other) noexcept
    {
        fd_ = other.fd_;
        other.fd_ = -1;
    }

    FileHandle& FileHandle::operator=(FileHandle&& other) noexcept
    {
        if (this != &other) {
            if (fd_ >= 0)
                ::close(fd_);
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    void FileHandle::reset()
    {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    FileHandle FileManager::open(const std::string& path, const FileOptions& opts,
                                 bool createIfMissing)
    {
        int flags = O_RDWR;
#ifdef O_CLOEXEC
        flags |= O_CLOEXEC;
#endif
        if (opts.direct_io) {
#ifdef O_DIRECT
            flags |= O_DIRECT;
#endif
        }
        int mode = 0644;
        int fd = ::open(path.c_str(), flags);
        if (fd < 0 && createIfMissing && errno == ENOENT) {
            fd = ::open(path.c_str(), flags | O_CREAT, mode);
        }
        if (fd < 0) {
            throw std::runtime_error(std::string("open(") + path + ")" + ": " +
                                     std::strerror(errno));
        }

        if (opts.sparse) {
#ifdef FALLOC_FL_KEEP_SIZE
            // no-op toggle; actual sparseness is FS-specific
#endif
        }
        FileHandle fh(fd);
        if (opts.preallocate_bytes) {
            preallocate(fh, opts.preallocate_bytes);
        }
        return fh;
    }

    void FileManager::preallocate(const FileHandle& fh, std::uint64_t lengthBytes)
    {
#ifdef __linux__
#ifdef FALLOC_FL_KEEP_SIZE
        if (::fallocate(fh.fd(), FALLOC_FL_KEEP_SIZE, 0, static_cast<off_t>(lengthBytes)) < 0) {
            // fallback to posix_fallocate or manual
#ifdef _POSIX_C_SOURCE
            int rc = ::posix_fallocate(fh.fd(), 0, static_cast<off_t>(lengthBytes));
            if (rc != 0)
                throw std::runtime_error(std::string("posix_fallocate: ") + std::strerror(rc));
#else
            // manual extend
            if (::lseek(fh.fd(), static_cast<off_t>(lengthBytes - 1), SEEK_SET) < 0)
                throw_io("lseek");
            if (::write(fh.fd(), "", 1) != 1)
                throw_io("write");
#endif
        }
#else
        int rc = ::posix_fallocate(fh.fd(), 0, static_cast<off_t>(lengthBytes));
        if (rc != 0)
            throw std::runtime_error(std::string("posix_fallocate: ") + std::strerror(rc));
#endif
#else
        (void)fh;
        (void)lengthBytes;
#endif
    }

    void FileManager::flush(const FileHandle& fh)
    {
        for (;;) {
            if (::fsync(fh.fd()) == 0)
                break;
            if (errno == EINTR)
                continue;
            throw_io("fsync");
        }
    }

    std::size_t FileManager::pwrite(const FileHandle& fh, const void* buf, std::size_t len,
                                    std::uint64_t off)
    {
        ssize_t n = ::pwrite(fh.fd(), buf, len, static_cast<off_t>(off));
        if (n < 0)
            throw_io("pwrite");
        return static_cast<std::size_t>(n);
    }

    std::size_t FileManager::pread(const FileHandle& fh, void* buf, std::size_t len,
                                   std::uint64_t off)
    {
        ssize_t n = ::pread(fh.fd(), buf, len, static_cast<off_t>(off));
        if (n < 0)
            throw_io("pread");
        return static_cast<std::size_t>(n);
    }

    std::size_t FileManager::io_alignment()
    {
        // Prefer 4096 as safe default for O_DIRECT. Could query with statfs/ioctl if needed.
        return 4096;
    }

    void FileManager::prefetch_willneed(const FileHandle& fh, std::uint64_t off, std::size_t len)
    {
#ifdef __linux__
#ifdef POSIX_FADV_WILLNEED
        // Best-effort; ignore errors
        ::posix_fadvise(fh.fd(), static_cast<off_t>(off), static_cast<off_t>(len),
                        POSIX_FADV_WILLNEED);
#else
        (void)fh;
        (void)off;
        (void)len;
#endif
#else
        (void)fh;
        (void)off;
        (void)len;
#endif
    }

    void FileMap::set_base_path(const std::string& dir, const std::string& baseName)
    {
        dir_ = dir;
        base_ = baseName;
    }

    std::string FileMap::segment_path(std::size_t index) const
    {
        return dir_ + "/" + base_ + ".seg" + std::to_string(index);
    }

    void FileMap::ensure_capacity(std::uint64_t logicalPage) const
    {
        const std::size_t neededSeg =
            static_cast<std::size_t>(logicalPage / layout_.pages_per_segment);
        while (segments_.size() <= neededSeg) {
            const std::size_t idx = segments_.size();
            std::string path = segment_path(idx);
            FileHandle fh = FileManager::open(path, layout_.options, /*create*/ true);
            // preallocate one segment worth of bytes
            std::uint64_t segBytes =
                static_cast<std::uint64_t>(layout_.pages_per_segment) * layout_.page_size;
            if (segBytes > 0)
                FileManager::preallocate(fh, segBytes);
            segments_.push_back(Segment{path, std::move(fh)});
        }
    }

    std::pair<std::size_t, std::uint64_t> FileMap::map(std::uint64_t logicalPage) const
    {
        std::size_t seg = static_cast<std::size_t>(logicalPage / layout_.pages_per_segment);
        std::uint64_t pageInSeg = logicalPage % layout_.pages_per_segment;
        std::uint64_t off = pageInSeg * static_cast<std::uint64_t>(layout_.page_size);
        return {seg, off};
    }

    void FileMap::write_page(std::uint64_t logicalPage, const void* pageData)
    {
        ensure_capacity(logicalPage);
        auto [seg, off] = map(logicalPage);
        if (seg >= segments_.size() || !segments_[seg].handle.valid()) {
            throw std::runtime_error(
                "FileMap: invalid handle for segment " + std::to_string(seg) +
                ", logicalPage=" + std::to_string(logicalPage) +
                ", path=" + (seg < segments_.size() ? segments_[seg].path : std::string("<none>")));
        }
        FileManager::pwrite(segments_[seg].handle, pageData, layout_.page_size, off);
    }

    void FileMap::read_page(std::uint64_t logicalPage, void* outBuffer) const
    {
        ensure_capacity(logicalPage);
        auto [seg, off] = map(logicalPage);
        FileManager::pread(segments_[seg].handle, outBuffer, layout_.page_size, off);
    }

} // namespace scratchbird::engine
