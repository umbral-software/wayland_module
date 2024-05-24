module;
#include <xkbcommon/xkbcommon.h>

#include <concepts>
#include <memory>
export module xkb;

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

export template<XkbDeletable T>
using XkbPointer = std::unique_ptr<T, XkbDeleter>;
