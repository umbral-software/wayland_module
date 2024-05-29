module;
#include <poll.h>
#include <wayland-cursor.h>
#include "wayland-xdg-shell-client-protocol.h"
#include "wayland-xdg-decoration-client-protocol.h"
#include <xkbcommon/xkbcommon.h>

#include <cerrno>
#include <cstdint>
#include <forward_list>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
export module wayland:display;

import :common;
import :seat;
import xkb;

static constexpr char DEFAULT_CURSOR_NAME[] = "default";
static constexpr std::uint8_t DEFAULT_CURSOR_SIZE = 16;

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

static short poll_single(int fd, short events, int timeout) {
    pollfd pfd { .fd = fd, .events = events };
    poll(&pfd, 1, timeout);
    return pfd.revents;
}

export class Display final : public IDisplayInternal {
public:
    Display() {
        static const wl_registry_listener registry_listener {
            .global = [](void *data, wl_registry *, std::uint32_t name, const char *interface, std::uint32_t version) noexcept {
                auto& self = *static_cast<Display *>(data);
                self._globals.emplace(name, std::make_pair(interface, version));
            },
            .global_remove = [](void *data, wl_registry *, std::uint32_t name) noexcept {
                auto& self = *static_cast<Display *>(data);
                self._globals.erase(name);
            }
        };

        static const xdg_wm_base_listener wm_base_listener {
            .ping = [](void *data, xdg_wm_base *wm_base, std::uint32_t serial) noexcept {
                xdg_wm_base_pong(wm_base, serial);
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
                _seats.emplace_front(*static_cast<IDisplayInternal*>(this), static_cast<wl_seat *>(wl_registry_bind(_registry.get(), name, &wl_seat_interface, WL_SEAT_DESIRED_VERSION)));
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

        xdg_wm_base_add_listener(_wm_base.get(), &wm_base_listener, this);

        _xkb_context.reset(xkb_context_new(XKB_CONTEXT_NO_FLAGS));
    }

    Display(const Display&) = delete;
    Display(Display&&) noexcept = delete;
    ~Display() = default;

    Display& operator=(const Display&) = delete;
    Display& operator=(Display&&) noexcept = delete;

    wl_display *handle() noexcept { return _display.get(); }
    const wl_display *handle() const noexcept { return _display.get(); }

    void poll_events() {
        while (wl_display_prepare_read(_display.get())) {
            wl_display_dispatch_pending(_display.get());
        }
        while (wl_display_flush(_display.get()) < 0 && EAGAIN == errno) {
            poll_single(wl_display_get_fd(_display.get()), POLLOUT, -1);
        }

        if (POLLIN & poll_single(wl_display_get_fd(_display.get()), POLLIN, 0)) {
            wl_display_read_events(_display.get());
            wl_display_dispatch_pending(_display.get());
        } else {
            wl_display_cancel_read(_display.get());
        }

        if (wl_display_get_error(_display.get())) {
            throw std::runtime_error("Wayland protocol error");
        }
        
        for (auto *pWindow : _windows) {
            pWindow->render_internal();
        }
    }

private:
    void register_window(IWindowInternal *window) { _windows.emplace_back(window); }
    void unregister_window(IWindowInternal *window) {
        for (auto i = _windows.begin(); i != _windows.end(); ++i) {
            if (*i == window) {
                _windows.erase(i);
                return;
            }
        }
        throw std::runtime_error("Attempted to unregister a window that doesn't exist");
    }

    void set_cursor(wl_pointer *pointer, std::uint32_t serial) const noexcept { wl_pointer_set_cursor(pointer, serial, _cursor_surface.get(), _cursor_hotspot.first, _cursor_hotspot.second); }

    wl_compositor *compositor() noexcept { return _compositor.get(); }
    const wl_compositor *compositor() const noexcept { return _compositor.get(); };
    
    zxdg_decoration_manager_v1 *decoration_manager() noexcept { return _decoration_manager.get(); };
    const zxdg_decoration_manager_v1 *decoration_manager() const noexcept { return _decoration_manager.get(); }

    wl_shm *shm() noexcept { return _shm.get(); }
    const wl_shm *shm() const noexcept { return _shm.get(); }

    xdg_wm_base *window_manager() noexcept { return _wm_base.get(); }
    const xdg_wm_base *window_manager() const noexcept { return _wm_base.get(); }

    xkb_context *xkb() noexcept { return _xkb_context.get(); }
    const xkb_context *xkb() const noexcept { return _xkb_context.get(); }

    IWindowInternal *window_from_handle(wl_surface *surface) noexcept {
        for (auto *pWindow : _windows) {
            if (pWindow->handle() == surface) {
                return pWindow;
            }
        }
        return nullptr;
    }

    const IWindowInternal *window_from_handle(wl_surface *surface) const noexcept {
        for (const auto *pWindow : _windows) {
            if (pWindow->handle() == surface) {
                return pWindow;
            }
        }
        return nullptr;
    }
private:
    WaylandPointer<wl_display> _display;
    WaylandPointer<wl_registry> _registry;
    std::map<std::uint32_t, std::pair<std::string, std::uint32_t>> _globals;

    WaylandPointer<wl_compositor> _compositor;
    std::forward_list<Seat> _seats;
    WaylandPointer<wl_shm> _shm;
    WaylandPointer<xdg_wm_base> _wm_base;
    WaylandPointer<zxdg_decoration_manager_v1> _decoration_manager; // Optional

    WaylandPointer<wl_surface> _cursor_surface;
    WaylandPointer<wl_cursor_theme> _cursor_theme;
    std::pair<std::uint32_t, std::uint32_t> _cursor_hotspot;

    XkbPointer<xkb_context> _xkb_context;

    std::vector<IWindowInternal *> _windows;
};
