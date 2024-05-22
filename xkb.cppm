module;
#include <xkbcommon/xkbcommon.h>
export module xkb;

export {
    using ::xkb_context;
    using ::xkb_context_new;
    using ::xkb_context_unref;
    using ::xkb_keymap;
    using ::xkb_keymap_mod_get_index;
    using ::xkb_keymap_new_from_buffer;
    using ::xkb_keymap_unref;
    using ::xkb_keysym_t;
    using ::xkb_state;
    using ::xkb_state_new;
    using ::xkb_state_key_get_syms;
    using ::xkb_state_key_get_utf8;
    using ::xkb_state_mod_index_is_active;
    using ::xkb_state_unref;
    using ::xkb_state_update_mask;
}
