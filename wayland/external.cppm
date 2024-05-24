module;
#define static // Hack to allow exporting wayland wrapper stubs from a module
#include "wayland-xdg-shell-client-protocol.h"
#include "wayland-xdg-decoration-client-protocol.h"
#undef static
#include <wayland-cursor.h>
export module wayland:external;

export {
    using ::wl_array;
    using ::wl_buffer;
    using ::wl_buffer_add_listener;
    using ::wl_buffer_destroy;
    using ::wl_buffer_listener;
    using ::wl_compositor;
    using ::wl_compositor_create_surface;
    using ::wl_compositor_destroy;
    using ::wl_compositor_interface;
    using ::wl_cursor_image_get_buffer;
    using ::wl_cursor_theme;
    using ::wl_cursor_theme_destroy;
    using ::wl_cursor_theme_get_cursor;
    using ::wl_cursor_theme_load;
    using ::wl_display;
    using ::wl_display_cancel_read;
    using ::wl_display_connect;
    using ::wl_display_disconnect;
    using ::wl_display_dispatch_pending;
    using ::wl_display_flush;
    using ::wl_display_get_error;
    using ::wl_display_get_fd;
    using ::wl_display_get_registry;
    using ::wl_display_prepare_read;
    using ::wl_display_read_events;
    using ::wl_display_roundtrip;
    using ::wl_keyboard;
    using ::wl_keyboard_add_listener;
    using ::wl_keyboard_listener;
    using ::wl_keyboard_release;
    using ::wl_pointer;
    using ::wl_pointer_add_listener;
    using ::wl_pointer_release;
    using ::wl_pointer_set_cursor;
    using ::wl_pointer_listener;
    using ::wl_proxy;
    using ::wl_proxy_get_id;
    using ::wl_registry;
    using ::wl_registry_add_listener;
    using ::wl_registry_bind;
    using ::wl_registry_destroy;
    using ::wl_registry_listener;
    using ::wl_seat;
    using ::wl_seat_add_listener;
    using ::wl_seat_get_pointer;
    using ::wl_seat_get_keyboard;
    using ::wl_seat_interface;
    using ::wl_seat_listener;
    using ::wl_seat_release;
    using ::wl_shm;
    using ::wl_shm_create_pool;
    using ::wl_shm_destroy;
    using ::wl_shm_interface;
    using ::wl_shm_pool;
    using ::wl_shm_pool_create_buffer;
    using ::wl_shm_pool_destroy;
    using ::wl_shm_pool_resize;
    using ::wl_surface;
    using ::wl_surface_attach;
    using ::wl_surface_commit;
    using ::wl_surface_damage;
    using ::wl_surface_damage_buffer;
    using ::wl_surface_destroy;
    using ::xdg_surface;
    using ::xdg_surface_ack_configure;
    using ::xdg_surface_add_listener;
    using ::xdg_surface_destroy;
    using ::xdg_surface_get_toplevel;
    using ::xdg_surface_listener;
    using ::xdg_toplevel;
    using ::xdg_toplevel_add_listener;
    using ::xdg_toplevel_destroy;
    using ::xdg_toplevel_listener;
    using ::xdg_toplevel_set_fullscreen;
    using ::xdg_toplevel_set_maximized;
    using ::xdg_toplevel_set_max_size;
    using ::xdg_toplevel_set_min_size;
    using ::xdg_toplevel_set_title;
    using ::xdg_toplevel_unset_fullscreen;
    using ::xdg_wm_base;
    using ::xdg_wm_base_add_listener;
    using ::xdg_wm_base_destroy;
    using ::xdg_wm_base_interface;
    using ::xdg_wm_base_get_xdg_surface;
    using ::xdg_wm_base_listener;
    using ::xdg_wm_base_pong;
    using ::zxdg_decoration_manager_v1;
    using ::zxdg_decoration_manager_v1_destroy;
    using ::zxdg_decoration_manager_v1_interface;
    using ::zxdg_decoration_manager_v1_get_toplevel_decoration;
    using ::zxdg_toplevel_decoration_v1;
    using ::zxdg_toplevel_decoration_v1_add_listener;
    using ::zxdg_toplevel_decoration_v1_destroy;
    using ::zxdg_toplevel_decoration_v1_listener;
    using ::zxdg_toplevel_decoration_v1_set_mode;
}
