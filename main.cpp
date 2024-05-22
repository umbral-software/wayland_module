import std;
import wayland;
import xkb;

#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-names.h>

// 4 has universal support, and damage_buffer
// 5 moves surface offsets to own request (not attach) loses support for Mir
// 6 adds integer scaling support, loses support for Weston and Steamdeck
static constexpr std::uint32_t WL_COMPOSITOR_DESIRED_VERSION = 4;

// 7 has universal support, but no specific features we need.
// 8 moves from wl_pointer::axis_discrete to wl_pointer::axis_value120, loses support for Weston
// 9 adds wl_pointer::axis_relative_direction, loses support for GNOME, Sway and Mir
static constexpr std::uint32_t WL_SEAT_DESIRED_VERSION = 7;

// Only known version
static constexpr std::uint32_t WL_SHM_DESIRED_VERSION = 1;

// Only known version
static constexpr std::uint32_t XDG_DECORATION_V1_DESIRED_VERSION = 1;

// 2 has universal support, but no specific features we need.
// 3 adds xdg_popup/xdg_positioner stuff, loses support for Sway
// 4 adds configure_bounds, loses support for Steamdeck
// 5 adds wm_capabilities
// 6 adds toplevel suspended state, loses support for Weston and Mir
static constexpr std::uint32_t XDG_SHELL_DESIRED_VERSION = 2;

static constexpr std::size_t BYTES_PER_PIXEL = sizeof(std::uint32_t);
static constexpr char DEFAULT_CURSOR_NAME[] = "default";
static constexpr std::uint8_t DEFAULT_CURSOR_SIZE = 16;
static constexpr std::pair<std::int32_t, std::int32_t> MIN_WINDOW_SIZE = { 800, 600 };
static constexpr std::size_t WAYLAND_TO_X_KEYCODE_OFFSET = 8;
static constexpr char WINDOW_TITLE[] = "Hello Wayland";

struct WaylandDeleter {
    void operator()(wl_buffer *buffer) const noexcept {
        wl_buffer_destroy(buffer);
    }
    void operator()(wl_compositor *compositor) const noexcept {
        wl_compositor_destroy(compositor);
    }
    void operator()(wl_cursor_theme *cursor_theme) const noexcept {
        wl_cursor_theme_destroy(cursor_theme);
    }
    void operator()(wl_display *display) const noexcept {
        wl_display_disconnect(display);
    }
    void operator()(wl_keyboard *keyboard) const noexcept {
        wl_keyboard_release(keyboard);
    }
    void operator()(wl_registry *registry) const noexcept {
        wl_registry_destroy(registry);
    }
    void operator()(wl_pointer *pointer) const noexcept {
        wl_pointer_release(pointer);
    }
    void operator()(wl_seat *seat) const noexcept {
        wl_seat_release(seat);
    }
    void operator()(wl_shm *shm) const noexcept {
        wl_shm_destroy(shm);
    }
    void operator()(wl_shm_pool *shm_pool) const noexcept {
        wl_shm_pool_destroy(shm_pool);
    }
    void operator()(wl_surface *surface) const noexcept {
        wl_surface_destroy(surface);
    }
    void operator()(xdg_surface *surface) const noexcept {
        xdg_surface_destroy(surface);
    }
    void operator()(xdg_toplevel *toplevel) const noexcept {
        xdg_toplevel_destroy(toplevel);
    }
    void operator()(xdg_wm_base *wm_base) const noexcept {
        xdg_wm_base_destroy(wm_base);
    }
    void operator()(zxdg_decoration_manager_v1 *decoration_manager) const noexcept {
        zxdg_decoration_manager_v1_destroy(decoration_manager);
    }
    void operator()(zxdg_toplevel_decoration_v1 *toplevel_decoration) const noexcept {
        zxdg_toplevel_decoration_v1_destroy(toplevel_decoration);
    }
};

template<typename T>
concept WaylandDeletable = std::invocable<WaylandDeleter, T*>;

template<WaylandDeletable T>
using WaylandPointer = std::unique_ptr<T, WaylandDeleter>;

struct XkbDeleter {
    void operator()(xkb_context *context) {
        xkb_context_unref(context);
    }
    void operator()(xkb_keymap *keymap) {
        xkb_keymap_unref(keymap);
    }
    void operator()(xkb_state *state) {
        xkb_state_unref(state);
    }
};

template<typename T>
concept XkbDeletable = std::invocable<XkbDeleter, T*>;

template<XkbDeletable T>
using XkbPointer = std::unique_ptr<T, XkbDeleter>;

class Buffer {
public:
    explicit Buffer(wl_shm *shm, std::pair<std::int32_t, std::int32_t> size) {
        _fd = memfd_create("framebuffer", MFD_CLOEXEC | MFD_ALLOW_SEALING | MFD_NOEXEC_SEAL);

        _filesize = BYTES_PER_PIXEL * size.first * size.second;
        ftruncate(_fd, _filesize);

        _filedata = static_cast<std::byte*>(mmap(nullptr, _filesize, PROT_WRITE, MAP_SHARED_VALIDATE, _fd, 0));
        fcntl(_fd, F_ADD_SEALS, F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_EXEC);

        _pool.reset(wl_shm_create_pool(shm, _fd, _filesize));

        _buffersize = _filesize;
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
        const auto required_buffersize = BYTES_PER_PIXEL * size.first * size.second;
        if (required_buffersize > _filesize) {
            ftruncate(_fd, required_buffersize);
            _filedata = mremap(_filedata, _filesize, required_buffersize, MREMAP_MAYMOVE);
            _filesize = required_buffersize;
            wl_shm_pool_resize(_pool.get(), _filesize);
        }
        if (required_buffersize != _buffersize) {
            _buffersize = required_buffersize;
            _buffer.reset(wl_shm_pool_create_buffer(_pool.get(), 0, size.first, size.second, BYTES_PER_PIXEL * size.first, WL_SHM_FORMAT_XRGB8888));
        }
        std::memset(_filedata, color, BYTES_PER_PIXEL * size.first * size.second);
    }

    wl_buffer *handle() noexcept {
        return _buffer.get();
    }

private:
    int _fd;
    void *_filedata;
    std::size_t _filesize, _buffersize;

    WaylandPointer<wl_shm_pool> _pool;
    WaylandPointer<wl_buffer> _buffer;
};

struct Seat {
    WaylandPointer<wl_seat> seat;
    WaylandPointer<wl_keyboard> keyboard;
    WaylandPointer<wl_pointer> pointer;
};

class Window {
public:
    Window();

    void poll_events();
    void render(std::uint8_t color);
    bool should_close() const noexcept;

private:
    WaylandPointer<wl_display> _display;
    WaylandPointer<wl_registry> _registry;
    std::map<std::uint32_t, std::pair<std::string, std::uint32_t>> _globals;

    WaylandPointer<wl_compositor> _compositor;
    std::vector<Seat> _seats;
    WaylandPointer<wl_shm> _shm;
    WaylandPointer<xdg_wm_base> _wm_base;
    WaylandPointer<zxdg_decoration_manager_v1> _decoration_manager; // Optional

    WaylandPointer<wl_surface> _cursor_surface;
    WaylandPointer<wl_cursor_theme> _cursor_theme;
    std::pair<std::uint32_t, std::uint32_t> _cursor_hotspot;

    XkbPointer<xkb_context> _xkb_context;
    XkbPointer<xkb_state> _xkb_state;
    int _xkb_alt_index;

    WaylandPointer<wl_surface> _wl_surface;
    WaylandPointer<xdg_surface> _xdg_surface;
    WaylandPointer<xdg_toplevel> _toplevel;
    WaylandPointer<zxdg_toplevel_decoration_v1> _toplevel_decoration; // Optional

    std::vector<Buffer> _usable_buffers;
    std::vector<Buffer> _in_use_buffers;

    std::pair<std::int32_t, std::int32_t> _requested_size = {0, 0};
    std::pair<std::int32_t, std::int32_t> _actual_size = {0, 0};
    bool _have_server_decorations = false;
    bool _is_fullscreen = false;
    bool _closed = false;
};

Window::Window() {
    static const wl_keyboard_listener keyboard_listener {
        .keymap = [](void *data, wl_keyboard *, std::uint32_t format, int fd, std::uint32_t size){
            auto& self = *static_cast<Window *>(data);

            switch (format) {
            case WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1: {
                void *keymap_data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
                XkbPointer<xkb_keymap> keymap(xkb_keymap_new_from_buffer(self._xkb_context.get(), static_cast<const char *>(keymap_data), size, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS));
                munmap(keymap_data, size);

                self._xkb_state.reset(xkb_state_new(keymap.get()));
                self._xkb_alt_index = xkb_keymap_mod_get_index(keymap.get(), XKB_MOD_NAME_ALT);
                break;
            }
            default:
                break;
            }
            close(fd);
        },
        .enter = [](void *, wl_keyboard *, std::uint32_t, wl_surface *, wl_array *){
            
        },
        .leave = [](void *, wl_keyboard *, std::uint32_t, wl_surface *){
            
        },
        .key = [](void *data, wl_keyboard *, std::uint32_t, std::uint32_t, std::uint32_t key, std::uint32_t state){
            auto& self = *static_cast<Window *>(data);
            const auto keycode = key + WAYLAND_TO_X_KEYCODE_OFFSET;

            if (WL_KEYBOARD_KEY_STATE_PRESSED == state) {
                // Convert from evdev to X keycode
                // Because X11 thought using keys 0-7 for mouse input was clever
                const xkb_keysym_t *keysyms;
                const auto num_keysyms = xkb_state_key_get_syms(self._xkb_state.get(), keycode, &keysyms);

                for (std::size_t i = 0; i < num_keysyms; ++i) {
                    switch (keysyms[i]) {
                    case XKB_KEY_Return:
                        if (xkb_state_mod_index_is_active(self._xkb_state.get(), self._xkb_alt_index, XKB_STATE_MODS_EFFECTIVE)) {
                            if (self._have_server_decorations) {
                                if (self._is_fullscreen) {
                                    xdg_toplevel_unset_fullscreen(self._toplevel.get());
                                    self._is_fullscreen = false;
                                } else {
                                    xdg_toplevel_set_fullscreen(self._toplevel.get(), nullptr);
                                    self._is_fullscreen = true;
                                }
                            }
                            break;
                        } else {
                            std::putchar('\n');
                        }
                        break;
                    default:
                        break;
                    }
                }

                const auto utf8_size = xkb_state_key_get_utf8(self._xkb_state.get(), keycode, nullptr, 0);
                if (utf8_size > 0) {
                    const auto text = std::make_unique_for_overwrite<char[]>(utf8_size + 1);
                    xkb_state_key_get_utf8(self._xkb_state.get(), keycode, text.get(), utf8_size + 1);
                    std::printf("%s", text.get());
                }
            }
        },
        .modifiers = [](void *data, wl_keyboard *, std::uint32_t, std::uint32_t mods_depressed, std::uint32_t mods_latched, std::uint32_t mods_locked, std::uint32_t group){
            auto& self = *static_cast<Window *>(data);
            xkb_state_update_mask(self._xkb_state.get(), mods_depressed, mods_latched, mods_locked, 0, 0, group);
        },
        .repeat_info = [](void *, wl_keyboard *, std::int32_t, std::int32_t){
            
        }
    };

    static const wl_pointer_listener pointer_listener {
        .enter = [](void *data, wl_pointer *pointer, std::uint32_t serial, wl_surface *, std::int32_t, std::int32_t){
            auto& self = *static_cast<Window *>(data);
            
           wl_pointer_set_cursor(pointer, serial, self._cursor_surface.get(), self._cursor_hotspot.first, self._cursor_hotspot.second);
        },
        .leave = [](void *, wl_pointer *, std::uint32_t, wl_surface *) {
            
        },
        .motion = [](void *, wl_pointer *, std::uint32_t, std::int32_t, std::int32_t) {

        },
        .button = [](void *, wl_pointer *, std::uint32_t, std::uint32_t, std::uint32_t button, std::uint32_t state) {
            if (BTN_LEFT == button) {
                switch (state) {
                case WL_POINTER_BUTTON_STATE_PRESSED:
                    std::puts("click");
                    break;
                case WL_POINTER_BUTTON_STATE_RELEASED:
                    std::puts("release");
                    break;
                }
            }
        },
        .axis = [](void *, wl_pointer *, std::uint32_t, std::uint32_t, std::int32_t) {

        },
        .frame = [](void *, wl_pointer *) {

        },
        .axis_source = [](void *, wl_pointer *, std::uint32_t) {

        },
        .axis_stop = [](void *, wl_pointer *, std::uint32_t, std::uint32_t) {

        },
        .axis_discrete = [](void *, wl_pointer *, std::uint32_t, std::int32_t) {

        }
    };

    static const wl_registry_listener registry_listener {
        .global = [](void *data, wl_registry *, std::uint32_t name, const char *interface, std::uint32_t version) noexcept {
            auto& self = *static_cast<Window *>(data);
            self._globals.emplace(name, std::make_pair(interface, version));
        },
        .global_remove = [](void *data, wl_registry *, std::uint32_t name) noexcept {
            auto& self = *static_cast<Window *>(data);
            self._globals.erase(name);
        }
    };

    static const wl_seat_listener seat_listener {
        .capabilities = [](void *data, wl_seat *wl_seat, std::uint32_t capabilities){
            auto& self = *static_cast<Window *>(data);
            for (auto& seat : self._seats) {
                if (seat.seat.get() == wl_seat) {
                    const auto has_pointer = !!(capabilities & WL_SEAT_CAPABILITY_POINTER);
                    if (has_pointer && !seat.pointer) {
                        seat.pointer.reset(wl_seat_get_pointer(wl_seat));
                        wl_pointer_add_listener(seat.pointer.get(), &pointer_listener, data);
                    } else if (!has_pointer && seat.pointer) {
                        seat.pointer.reset();
                    }

                    const auto has_keyboard = !!(capabilities && WL_SEAT_CAPABILITY_KEYBOARD);
                    if (has_keyboard & !seat.keyboard) {
                        seat.keyboard.reset(wl_seat_get_keyboard(wl_seat));
                        wl_keyboard_add_listener(seat.keyboard.get(), &keyboard_listener, data);
                    } else if (!has_keyboard && seat.keyboard) {
                        seat.keyboard.reset();
                    }
                }
            }
        },
        .name = [](void *, wl_seat *, const char *){

        }
    };

    static const xdg_surface_listener surface_listener {
        .configure = [](void *data, xdg_surface *surface, std::uint32_t serial) noexcept {
            auto& self = *static_cast<Window *>(data);
            xdg_surface_ack_configure(surface, serial);
            self._actual_size = self._requested_size;
        }
    };

    static const xdg_toplevel_listener toplevel_listener {
        .configure = [](void *data, xdg_toplevel *, std::int32_t width, std::int32_t height, wl_array *) noexcept {
            auto& self = *static_cast<Window *>(data);
            self._requested_size = { width, height };
        },
        .close = [](void *data, xdg_toplevel *) noexcept {
            auto& self = *static_cast<Window *>(data);
            self._closed = true;
        }
    };

    static const xdg_wm_base_listener wm_base_listener {
        .ping = [](void *data, xdg_wm_base *wm_base, std::uint32_t serial) noexcept {
            xdg_wm_base_pong(wm_base, serial);
        }
    };

    static const zxdg_toplevel_decoration_v1_listener toplevel_decoration_listener {
        .configure = [](void *data, zxdg_toplevel_decoration_v1 *, std::uint32_t mode) noexcept {
            auto& self = *static_cast<Window *>(data);
            switch(mode) {
            default:
            case ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE:
                xdg_toplevel_set_fullscreen(self._toplevel.get(), nullptr);
                self._have_server_decorations = false;
                break;
            case ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE:
                self._have_server_decorations = true;
                break;
            }
        }
    };

    _display.reset(wl_display_connect(nullptr));
    if (!_display) {
        throw std::runtime_error("No Wayland Display found");
    }

    _registry.reset(wl_display_get_registry(_display.get()));
    wl_registry_add_listener(_registry.get(), &registry_listener, this);
    wl_display_roundtrip(_display.get());

    for (const auto& global : _globals) {
        const auto name = global.first;
        const auto& interface = global.second.first;
        const auto version = global.second.second;
        if ("wl_compositor" == interface && WL_COMPOSITOR_DESIRED_VERSION <= version) {
            _compositor.reset(static_cast<wl_compositor *>(wl_registry_bind(_registry.get(), name, &wl_compositor_interface, WL_COMPOSITOR_DESIRED_VERSION)));
        }
        if ("wl_seat" == interface && WL_SEAT_DESIRED_VERSION <= version) {
            _seats.emplace_back(WaylandPointer<wl_seat>{static_cast<wl_seat *>(wl_registry_bind(_registry.get(), name, &wl_seat_interface, WL_SEAT_DESIRED_VERSION))});
        }
        if ("wl_shm" == interface && WL_SHM_DESIRED_VERSION <= version) {
            _shm.reset(static_cast<wl_shm *>(wl_registry_bind(_registry.get(), name, &wl_shm_interface, WL_SHM_DESIRED_VERSION)));
        }
        if ("xdg_wm_base" == interface && XDG_SHELL_DESIRED_VERSION <= version) {
            _wm_base.reset(static_cast<xdg_wm_base *>(wl_registry_bind(_registry.get(), name, &xdg_wm_base_interface, XDG_SHELL_DESIRED_VERSION)));
        }
        if ("zxdg_decoration_manager_v1" == interface && XDG_DECORATION_V1_DESIRED_VERSION <= version) {
            _decoration_manager.reset(static_cast<zxdg_decoration_manager_v1 *>(wl_registry_bind(_registry.get(), name, &zxdg_decoration_manager_v1_interface, XDG_DECORATION_V1_DESIRED_VERSION)));
        }
    }

    if (!_compositor) {
        throw std::runtime_error("No Compositor Global found");
    }
    if (!_shm) {
        throw std::runtime_error("No SHM Global found");
    }
    if (!_wm_base) {
        throw std::runtime_error("No XDG Shell Global found");
    }

    std::uint8_t cursor_size;
    const auto xcursor_size = std::getenv("XCURSOR_SIZE");
    if (xcursor_size) {
        cursor_size = std::atoi(xcursor_size);
    } else {
        cursor_size = DEFAULT_CURSOR_SIZE;
    }

    _cursor_surface.reset(wl_compositor_create_surface(_compositor.get()));
    _cursor_theme.reset(wl_cursor_theme_load(nullptr, cursor_size, _shm.get()));
    const auto cursor = wl_cursor_theme_get_cursor(_cursor_theme.get(), DEFAULT_CURSOR_NAME);
    const auto cursor_image = cursor->images[0];
    _cursor_hotspot = {cursor_image->hotspot_x, cursor_image->hotspot_y};
    wl_surface_attach(_cursor_surface.get(), wl_cursor_image_get_buffer(cursor_image), 0, 0);
    wl_surface_commit(_cursor_surface.get());

    _xkb_context.reset(xkb_context_new(XKB_CONTEXT_NO_FLAGS));

    for (const auto& seat : _seats) {
        wl_seat_add_listener(seat.seat.get(), &seat_listener, this);
    }

    xdg_wm_base_add_listener(_wm_base.get(), &wm_base_listener, this);

    _wl_surface.reset(wl_compositor_create_surface(_compositor.get()));
    _xdg_surface.reset(xdg_wm_base_get_xdg_surface(_wm_base.get(), _wl_surface.get()));
    xdg_surface_add_listener(_xdg_surface.get(), &surface_listener, this);

    _toplevel.reset(xdg_surface_get_toplevel(_xdg_surface.get()));
    xdg_toplevel_add_listener(_toplevel.get(), &toplevel_listener, this);
    xdg_toplevel_set_min_size(_toplevel.get(), MIN_WINDOW_SIZE.first, MIN_WINDOW_SIZE.second);
    xdg_toplevel_set_title(_toplevel.get(), WINDOW_TITLE);

    if (_decoration_manager) {
        _toplevel_decoration.reset(zxdg_decoration_manager_v1_get_toplevel_decoration(_decoration_manager.get(), _toplevel.get()));
        zxdg_toplevel_decoration_v1_add_listener(_toplevel_decoration.get(), &toplevel_decoration_listener, this);
        zxdg_toplevel_decoration_v1_set_mode(_toplevel_decoration.get(), ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    } else {
        xdg_toplevel_set_fullscreen(_toplevel.get(), nullptr);
        _is_fullscreen = true;
    }

    wl_surface_commit(_wl_surface.get());
}

void Window::poll_events() {
    while (wl_display_prepare_read(_display.get())) {
        wl_display_dispatch_pending(_display.get());
    }
    while (wl_display_flush(_display.get()) && errno == EAGAIN) {
        // TODO: Sleep in poll(POLLOUT) here?
    }
    // TODO: poll(POLLIN) here?
    wl_display_read_events(_display.get());
    wl_display_dispatch_pending(_display.get());

    if (wl_display_get_error(_display.get())) {
        throw std::runtime_error("Wayland protocol error");
    }
}

void Window::render(std::uint8_t color) {
    static const wl_buffer_listener buffer_listener = {
        .release = [](void *data, wl_buffer *buffer) {
            auto& self = *static_cast<Window *>(data);
            for (auto i = self._in_use_buffers.begin(); i != self._in_use_buffers.end(); ++i) {
                if (i->handle() == buffer) {
                    self._usable_buffers.emplace_back(std::move(*i));
                    self._in_use_buffers.erase(i);
                    return;
                }
            }
        }
    };

    _actual_size = std::make_pair(std::max(_requested_size.first, MIN_WINDOW_SIZE.first), std::max(_requested_size.second, MIN_WINDOW_SIZE.second));

    auto buffer = [this]() {
        if (!_usable_buffers.empty()) {
            auto buffer = std::move(_usable_buffers.back());
            _usable_buffers.pop_back();
            return buffer;
        } else {
            auto buffer = Buffer(_shm.get(), _actual_size);
            wl_buffer_add_listener(buffer.handle(), &buffer_listener, this);
            return buffer;
        }
    }();

    buffer.draw(_actual_size, color);

    wl_surface_attach(_wl_surface.get(), buffer.handle(), 0, 0);
    wl_surface_damage_buffer(_wl_surface.get(), 0, 0, _actual_size.first, _actual_size.second);
    wl_surface_commit(_wl_surface.get());

    _in_use_buffers.emplace_back(std::move(buffer));
}

bool Window::should_close() const noexcept {
    return _closed;
}

int main() {
    Window window;

    bool ascending = true;
    std::uint8_t color = 0xFF;
    while (!window.should_close()) {
        window.poll_events();
        window.render(color);

        color += ascending ? 1 : -1;
        if ((ascending && color == std::numeric_limits<std::uint8_t>::max())
        || (!ascending && color == std::numeric_limits<std::uint8_t>::min())) {
            ascending = !ascending;
        }
    }
}
