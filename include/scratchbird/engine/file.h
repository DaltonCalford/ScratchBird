#ifndef SCRATCHBIRD_ENGINE_FILE_H
#define SCRATCHBIRD_ENGINE_FILE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace scratchbird::engine
{

    struct FileOptions {
        bool direct_io{false};
        bool sparse{false};
        std::size_t preallocate_bytes{0};
    };

    class FileHandle
    {
      public:
        FileHandle() = default;
        explicit FileHandle(int fd) : fd_(fd) {}
        ~FileHandle();

        FileHandle(const FileHandle&) = delete;
        FileHandle& operator=(const FileHandle&) = delete;

        FileHandle(FileHandle&& other) noexcept;
        FileHandle& operator=(FileHandle&& other) noexcept;

        bool valid() const
        {
            return fd_ >= 0;
        }
        int fd() const
        {
            return fd_;
        }
        void reset();

      private:
        int fd_{-1};
    };

    class FileManager
    {
      public:
        // Create or open a file with options. createIfMissing controls creation.
        static FileHandle open(const std::string& path, const FileOptions& opts,
                               bool createIfMissing);

        // Preallocate file space to at least length bytes.
        static void preallocate(const FileHandle& fh, std::uint64_t lengthBytes);

        // Flush file to stable storage.
        static void flush(const FileHandle& fh);

        // Aligned positional read/write. Returns bytes processed or throws.
        static std::size_t pwrite(const FileHandle& fh, const void* buf, std::size_t len,
                                  std::uint64_t off);
        static std::size_t pread(const FileHandle& fh, void* buf, std::size_t len,
                                 std::uint64_t off);

        // Advise kernel about access pattern
        static void prefetch_willneed(const FileHandle& fh, std::uint64_t off, std::size_t len);

        // Recommended alignment in bytes for O_DIRECT (or system page size fallback).
        static std::size_t io_alignment();
    };

    // Maps logical page numbers to backing files and offsets.
    class FileMap
    {
      public:
        struct Segment {
            std::string path;
            FileHandle handle;
        };

        struct Layout {
            std::uint32_t page_size{4096};           // bytes per page
            std::uint64_t pages_per_segment{262144}; // default: ~1GB at 4KB pages
            FileOptions options{};
        };

        explicit FileMap(Layout layout) : layout_(layout) {}

        // Ensure at least segments covering up to and including logicalPage exist.
        void ensure_capacity(std::uint64_t logicalPage) const;

        // Map logical page to (segment index, byte offset within that segment file).
        std::pair<std::size_t, std::uint64_t> map(std::uint64_t logicalPage) const;

        // Read/write a full page at logicalPage.
        void write_page(std::uint64_t logicalPage, const void* pageData);
        void read_page(std::uint64_t logicalPage, void* outBuffer) const;

        // Manage root directory and base name for segments.
        void set_base_path(const std::string& dir, const std::string& baseName);

        const std::vector<Segment>& segments() const
        {
            return segments_;
        }

      private:
        std::string segment_path(std::size_t index) const;

        Layout layout_{};
        std::string dir_{};
        std::string base_{};
        mutable std::vector<Segment> segments_{};
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_FILE_H
