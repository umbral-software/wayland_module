export module wayland:common;

import :external;
import std;
import xkb;

struct WaylandDeleter {
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

export template<WaylandDeletable T>
using WaylandPointer = std::unique_ptr<T, WaylandDeleter>;

export class IWindowInternal {
protected:
    ~IWindowInternal() = default;
public:
    virtual wl_surface *handle() noexcept = 0;
    virtual const wl_surface *handle() const noexcept = 0;

    virtual void key_down(xkb_keysym_t key, bool alt, bool ctrl, bool shift) = 0;
    virtual void key_up(xkb_keysym_t key, bool alt, bool ctrl, bool shift) = 0;
    virtual void pointer_click(std::int32_t x, std::int32_t y) = 0;
    virtual void pointer_release(std::int32_t x, std::int32_t y) = 0;
    virtual void text(std::string_view string) = 0;

    virtual void render_internal() = 0;

    virtual bool toggle_fullscreen() noexcept = 0;
};

export class IDisplayInternal {
protected:
    ~IDisplayInternal() = default;
public:
    virtual void register_window(IWindowInternal *window) = 0;
    virtual void unregister_window(IWindowInternal *window) = 0;
    virtual void set_cursor(wl_pointer *pointer, std::uint32_t serial) const noexcept = 0;

    virtual wl_compositor *compositor() noexcept = 0;
    virtual const wl_compositor *compositor() const noexcept = 0;
    
    virtual zxdg_decoration_manager_v1 *decoration_manager() noexcept = 0;
    virtual const zxdg_decoration_manager_v1 *decoration_manager() const noexcept = 0;

    virtual wl_shm *shm() noexcept = 0;
    virtual const wl_shm *shm() const noexcept = 0;

    virtual xdg_wm_base *window_manager() noexcept = 0;
    virtual const xdg_wm_base *window_manager() const noexcept = 0;

    virtual xkb_context *xkb() noexcept = 0;
    virtual const xkb_context *xkb() const noexcept = 0;

    virtual IWindowInternal *window_from_handle(wl_surface *surface) noexcept = 0;
    virtual const IWindowInternal *window_from_handle(wl_surface *surface) const noexcept = 0;
};
