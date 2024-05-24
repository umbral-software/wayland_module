module;
#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-names.h>
export module simple_wayland:seat;

import :common;
import :window;
import simple_xkb;
import std;
import wayland;

static constexpr std::size_t WAYLAND_TO_X_KEYCODE_OFFSET = 8;

export class Seat {
public:
    Seat(IDisplayInternal& display, wl_seat *_seat);
    Seat(const Seat&) = delete;
    Seat(Seat&&) noexcept = delete;
    ~Seat() = default;

    Seat& operator=(const Seat&) = delete;
    Seat& operator=(Seat&&) noexcept = delete;
private:
    IDisplayInternal& _display;

    WaylandPointer<wl_seat> _seat;
    WaylandPointer<wl_keyboard> _keyboard;
    WaylandPointer<wl_pointer> _pointer;

    XkbPointer<xkb_state> _xkb_state;
    int _xkb_alt_index;

    IWindowInternal *_focus;
};

Seat::Seat(IDisplayInternal& display, wl_seat *seat)
    :_display(display)
    ,_seat(seat)
{
    static const wl_keyboard_listener keyboard_listener {
        .keymap = [](void *data, wl_keyboard *, std::uint32_t format, int fd, std::uint32_t size){
            auto& self = *static_cast<Seat *>(data);

            switch (format) {
            case WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1: {
                void *keymap_data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
                XkbPointer<xkb_keymap> keymap(xkb_keymap_new_from_buffer(self._display.xkb(), static_cast<const char *>(keymap_data), size, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS));
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
        .enter = [](void *data, wl_keyboard *keyboard, std::uint32_t, wl_surface *surface, wl_array *){
            auto& self = *static_cast<Seat *>(data);
            self._focus = self._display.window_from_handle(surface);
        },
        .leave = [](void *data, wl_keyboard *keyboard, std::uint32_t, wl_surface *){
            auto& self = *static_cast<Seat *>(data);
            self._focus = nullptr;
        },
        .key = [](void *data, wl_keyboard *, std::uint32_t, std::uint32_t, std::uint32_t key, std::uint32_t state){
            auto& self = *static_cast<Seat *>(data);
            // Convert from evdev to X keycode
            // Because X11 thought using keys 0-7 for mouse input was clever
            const auto keycode = key + WAYLAND_TO_X_KEYCODE_OFFSET;

            if (WL_KEYBOARD_KEY_STATE_PRESSED == state) {
                const xkb_keysym_t *keysyms;
                const auto num_keysyms = xkb_state_key_get_syms(self._xkb_state.get(), keycode, &keysyms);

                for (std::size_t i = 0; i < num_keysyms; ++i) {
                    switch (keysyms[i]) {
                    case XKB_KEY_Return:
                        if (xkb_state_mod_index_is_active(self._xkb_state.get(), self._xkb_alt_index, XKB_STATE_MODS_EFFECTIVE)) {
                            if (self._focus) {
                                self._focus->toggle_fullscreen();
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
            auto& self = *static_cast<Seat *>(data);
            xkb_state_update_mask(self._xkb_state.get(), mods_depressed, mods_latched, mods_locked, 0, 0, group);
        },
        .repeat_info = [](void *, wl_keyboard *, std::int32_t, std::int32_t){
            
        }
    };

    static const wl_pointer_listener pointer_listener {
        .enter = [](void *data, wl_pointer *pointer, std::uint32_t serial, wl_surface *, std::int32_t, std::int32_t){
            auto& self = *static_cast<Seat *>(data);
            
           self._display.set_cursor(pointer, serial);
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

    static const wl_seat_listener seat_listener {
        .capabilities = [](void *data, wl_seat *, std::uint32_t capabilities){
            auto& self = *static_cast<Seat *>(data);
            
            const auto has_pointer = !!(capabilities & WL_SEAT_CAPABILITY_POINTER);
            if (has_pointer && !self._pointer) {
                self._pointer.reset(wl_seat_get_pointer(self._seat.get()));
                wl_pointer_add_listener(self._pointer.get(), &pointer_listener, data);
            } else if (!has_pointer && self._pointer) {
                self._pointer.reset();
            }

            const auto has_keyboard = !!(capabilities && WL_SEAT_CAPABILITY_KEYBOARD);
            if (has_keyboard & !self._keyboard) {
                self._keyboard.reset(wl_seat_get_keyboard(self._seat.get()));
                wl_keyboard_add_listener(self._keyboard.get(), &keyboard_listener, data);
            } else if (!has_keyboard && self._keyboard) {
                self._keyboard.reset();
            }
        },
        .name = [](void *, wl_seat *, const char *){

        }
    };
    wl_seat_add_listener(_seat.get(), &seat_listener, this);
}
