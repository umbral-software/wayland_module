module;
#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon-names.h>

#include <cstdint>
#include <string>
#include <vector>
export module wayland:seat;

import :common;
import :external;
import :window;
import xkb;

static constexpr std::size_t WAYLAND_TO_X_KEYCODE_OFFSET = 8;

static std::vector<xkb_keysym_t> keycode_to_keysyms(xkb_state *state, xkb_keycode_t key) {
    const xkb_keysym_t *keysyms;
    const auto num_keysyms = xkb_state_key_get_syms(state, key, &keysyms);
    return std::vector(keysyms, keysyms + num_keysyms);
}

static std::string keycode_to_text(xkb_state *state, xkb_keycode_t key) {
    const auto num_chars = xkb_state_key_get_utf8(state, key, nullptr, 0);

    std::string ret(num_chars + 1, '\0');
    xkb_state_key_get_utf8(state, key, ret.data(), ret.size());
    return ret;
}

export class Seat {
public:
    Seat(IDisplayInternal& display, wl_seat *seat)
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

                    self._state.reset(xkb_state_new(keymap.get()));
                    self._alt_index = xkb_keymap_mod_get_index(keymap.get(), XKB_MOD_NAME_ALT);
                    self._ctrl_index = xkb_keymap_mod_get_index(keymap.get(), XKB_MOD_NAME_CTRL);
                    self._shift_index = xkb_keymap_mod_get_index(keymap.get(), XKB_MOD_NAME_SHIFT);
                    break;
                }
                default:
                    break;
                }
                close(fd);
            },
            .enter = [](void *data, wl_keyboard *keyboard, std::uint32_t, wl_surface *surface, wl_array *){
                auto& self = *static_cast<Seat *>(data);
                self._keyboard_focus = self._display.window_from_handle(surface);
            },
            .leave = [](void *data, wl_keyboard *keyboard, std::uint32_t, wl_surface *){
                auto& self = *static_cast<Seat *>(data);
                self._keyboard_focus = nullptr;
            },
            .key = [](void *data, wl_keyboard *, std::uint32_t, std::uint32_t, std::uint32_t key, std::uint32_t state){
                auto& self = *static_cast<Seat *>(data);
                if (self._keyboard_focus) {
                    // Convert from evdev to X keycode
                    // Because X11 thought using keys 0-7 for mouse input was clever
                    const auto keycode = key + WAYLAND_TO_X_KEYCODE_OFFSET;

                    const auto keys = keycode_to_keysyms(self._state.get(), keycode);
                    const auto mods = xkb_state_serialize_mods(self._state.get(), XKB_STATE_MODS_EFFECTIVE);

                    switch(state) {
                    case XKB_KEY_DOWN:
                        for (const auto key : keys) {
                            self._keyboard_focus->key_down(key,
                                xkb_state_mod_index_is_active(self._state.get(), self._alt_index, XKB_STATE_MODS_EFFECTIVE),
                                xkb_state_mod_index_is_active(self._state.get(), self._ctrl_index, XKB_STATE_MODS_EFFECTIVE),
                                xkb_state_mod_index_is_active(self._state.get(), self._shift_index, XKB_STATE_MODS_EFFECTIVE));
                        }
                        self._keyboard_focus->text(keycode_to_text(self._state.get(), keycode));
                        break;

                    case XKB_KEY_UP:
                        for (const auto key : keycode_to_keysyms(self._state.get(), keycode)) {
                            self._keyboard_focus->key_up(key,
                                xkb_state_mod_index_is_active(self._state.get(), self._alt_index, XKB_STATE_MODS_EFFECTIVE),
                                xkb_state_mod_index_is_active(self._state.get(), self._ctrl_index, XKB_STATE_MODS_EFFECTIVE),
                                xkb_state_mod_index_is_active(self._state.get(), self._shift_index, XKB_STATE_MODS_EFFECTIVE));
                        }
                        break;
                    }
                }
            },
            .modifiers = [](void *data, wl_keyboard *, std::uint32_t, std::uint32_t mods_depressed, std::uint32_t mods_latched, std::uint32_t mods_locked, std::uint32_t group){
                auto& self = *static_cast<Seat *>(data);
                xkb_state_update_mask(self._state.get(), mods_depressed, mods_latched, mods_locked, 0, 0, group);
            },
            .repeat_info = [](void *, wl_keyboard *, std::int32_t, std::int32_t){
                
            }
        };

        static const wl_pointer_listener pointer_listener {
            .enter = [](void *data, wl_pointer *pointer, std::uint32_t serial, wl_surface *surface, std::int32_t, std::int32_t){
                auto& self = *static_cast<Seat *>(data);
                
                self._display.set_cursor(pointer, serial);
                self._pointer_focus = self._display.window_from_handle(surface);
            },
            .leave = [](void *data, wl_pointer *, std::uint32_t, wl_surface *) {
                auto& self = *static_cast<Seat *>(data);

                self._pointer_focus = nullptr;
            },
            .motion = [](void *, wl_pointer *, std::uint32_t, std::int32_t, std::int32_t) {

            },
            .button = [](void *data, wl_pointer *, std::uint32_t x, std::uint32_t y, std::uint32_t button, std::uint32_t state) {
                auto& self = *static_cast<Seat *>(data);
                if (self._pointer_focus) {
                    if (BTN_LEFT == button) {
                        switch (state) {
                        case WL_POINTER_BUTTON_STATE_PRESSED:
                            self._pointer_focus->pointer_click(x, y);
                            break;
                        case WL_POINTER_BUTTON_STATE_RELEASED:
                            self._pointer_focus->pointer_release(x, y);
                            break;
                        }
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

    XkbPointer<xkb_state> _state;
    xkb_mod_index_t _alt_index, _ctrl_index, _shift_index;

    IWindowInternal *_keyboard_focus = nullptr;
    IWindowInternal *_pointer_focus = nullptr;
};
