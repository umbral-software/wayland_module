export module wayland:window;

import :buffer;
import :common;
import :external;
import std;

static constexpr std::pair<std::int32_t, std::int32_t> MIN_WINDOW_SIZE = { 800, 600 };

export class Window : public IWindowInternal {
public:
    Window(IDisplayInternal& display, const char *title)
        :_display(display)
    {
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

        static const xdg_surface_listener surface_listener {
            .configure = [](void *data, xdg_surface *surface, std::uint32_t serial) noexcept {
                auto& self = *static_cast<Window *>(data);
                xdg_surface_ack_configure(surface, serial);
                self.recalculate_size();
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

        _display.register_window(this);

        _wl_surface.reset(wl_compositor_create_surface(_display.compositor()));
        _xdg_surface.reset(xdg_wm_base_get_xdg_surface(_display.window_manager(), _wl_surface.get()));
        xdg_surface_add_listener(_xdg_surface.get(), &surface_listener, this);

        _toplevel.reset(xdg_surface_get_toplevel(_xdg_surface.get()));
        xdg_toplevel_add_listener(_toplevel.get(), &toplevel_listener, this);
        xdg_toplevel_set_min_size(_toplevel.get(), MIN_WINDOW_SIZE.first, MIN_WINDOW_SIZE.second);
        xdg_toplevel_set_title(_toplevel.get(), title);
        
        if (_display.decoration_manager()) {
            _toplevel_decoration.reset(zxdg_decoration_manager_v1_get_toplevel_decoration(_display.decoration_manager(), _toplevel.get()));
            zxdg_toplevel_decoration_v1_add_listener(_toplevel_decoration.get(), &toplevel_decoration_listener, this);
            zxdg_toplevel_decoration_v1_set_mode(_toplevel_decoration.get(), ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
        } else {
            xdg_toplevel_set_fullscreen(_toplevel.get(), nullptr);
            _is_fullscreen = true;
        }

        wl_surface_commit(_wl_surface.get());
    }

    Window(const Window&) = delete;
    Window(Window&&) noexcept = delete;
    ~Window() {
        _display.unregister_window(this);
    }

    Window& operator=(const Window&) = delete;
    Window& operator=(Window&&) noexcept = delete;

    wl_surface *handle() { return _wl_surface.get(); }
    const wl_surface *handle() const { return _wl_surface.get(); }

    void render(std::uint8_t color) {
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

        if (_actual_size.first == 0 || _actual_size.second == 0) {
            recalculate_size();
        }
        auto buffer = [this]() {
            if (!_usable_buffers.empty()) {
                auto buffer = std::move(_usable_buffers.back());
                _usable_buffers.pop_back();
                return buffer;
            } else {
                auto buffer = Buffer(_display.shm(), _actual_size);
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

    bool should_close() const noexcept { return _closed; }

    bool toggle_fullscreen() noexcept {
        if (_have_server_decorations) {
            if (_is_fullscreen) {
                xdg_toplevel_unset_fullscreen(_toplevel.get());
                _is_fullscreen = false;
            } else {
                xdg_toplevel_set_fullscreen(_toplevel.get(), nullptr);
                _is_fullscreen = true;
            }
            return true;
        } else {
            return false;
        }
    }

    void recalculate_size() {
        _actual_size = std::make_pair(
            std::max(_requested_size.first, MIN_WINDOW_SIZE.first),
            std::max(_requested_size.second, MIN_WINDOW_SIZE.second)
        );
    }

private:
    IDisplayInternal& _display;

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
