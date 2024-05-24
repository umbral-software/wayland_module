module;
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
export module wayland:buffer;

import :common;
import :external;
import std;

static constexpr std::size_t BYTES_PER_PIXEL = sizeof(std::uint32_t);

export class Buffer {
public:
    explicit Buffer(wl_shm *shm, std::pair<std::int32_t, std::int32_t> size) {
        _fd = memfd_create("framebuffer", MFD_CLOEXEC | MFD_ALLOW_SEALING | MFD_NOEXEC_SEAL);

        _filesize = BYTES_PER_PIXEL * size.first * size.second;
        ftruncate(_fd, _filesize);

        _filedata = static_cast<std::byte*>(mmap(nullptr, _filesize, PROT_WRITE, MAP_SHARED_VALIDATE, _fd, 0));
        fcntl(_fd, F_ADD_SEALS, F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_EXEC);

        _pool.reset(wl_shm_create_pool(shm, _fd, _filesize));

        _buffersize = size;
        _buffer.reset(wl_shm_pool_create_buffer(_pool.get(), 0, size.first, size.second, BYTES_PER_PIXEL * size.first, WL_SHM_FORMAT_XRGB8888));
    }

    Buffer(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept {
        _fd = other._fd;
        _filedata = other._filedata;
        _filesize = other._filesize;
        _buffersize = other._buffersize;

        _pool = std::move(other._pool);
        _buffer = std::move(other._buffer);

        other._filedata = nullptr;
    }

    ~Buffer() {
        if (_filedata) {
            munmap(_filedata, _filesize);
            close(_fd);
        }
    }

    Buffer& operator=(const Buffer&) = delete;
    Buffer& operator=(Buffer&& other) noexcept {
        if (_filedata) {
            munmap(_filedata, _filesize);
            close(_fd);
        }

        _fd = other._fd;
        _filedata = other._filedata;
        _filesize = other._filesize;
        _buffersize = other._buffersize;

        _pool = std::move(other._pool);
        _buffer = std::move(other._buffer);

        other._filedata = nullptr;
        return *this;
    }

    void draw(std::pair<std::int32_t, std::int32_t> size, std::uint8_t color) {
        const auto required_filesize = BYTES_PER_PIXEL * size.first * size.second;
        if (required_filesize > _filesize) {
            ftruncate(_fd, required_filesize);
            _filedata = mremap(_filedata, _filesize, required_filesize, MREMAP_MAYMOVE);
            _filesize = required_filesize;
            wl_shm_pool_resize(_pool.get(), _filesize);
        }
        
        std::memset(_filedata, color, required_filesize);

        if (size != _buffersize) {
            _buffer.reset(wl_shm_pool_create_buffer(_pool.get(), 0, size.first, size.second, BYTES_PER_PIXEL * size.first, WL_SHM_FORMAT_XRGB8888));
            _buffersize = size;
        }
    }

    wl_buffer *handle() noexcept {
        return _buffer.get();
    }

private:
    int _fd;
    void *_filedata;
    std::size_t _filesize;
    std::pair<std::int32_t, std::int32_t> _buffersize;

    WaylandPointer<wl_shm_pool> _pool;
    WaylandPointer<wl_buffer> _buffer;
};