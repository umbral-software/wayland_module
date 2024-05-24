import wayland;
import std;
import xkb;

#include <xkbcommon/xkbcommon-keysyms.h>

static constexpr char WINDOW_TITLE[] = "Hello Wayland";

class SimpleWindow final : public Window {
public:
    SimpleWindow(Display& display, const char *title)
        :Window(display, title)
    {

    }

    void key_down(xkb_keysym_t key, bool alt, bool ctrl, bool shift) override {
        if (key == XKB_KEY_Return)  {
            if (alt) {
                toggle_fullscreen();
            } else {
                std::putchar('\n');
            }
        }
    }
    void key_up(xkb_keysym_t key, bool alt, bool ctrl, bool shift) override {

    }
    void pointer_click(std::int32_t x, std::int32_t y) override {
        std::printf("CLICK  : %d %d\n", x, y);
    }
    void pointer_release(std::int32_t x, std::int32_t y) override {
        std::printf("RELEASE: %d %d\n", x, y);
    }
    void text(std::string_view string) override {
        std::printf("%s", string.data());
    }

    void render(void *buffer, std::pair<std::int32_t, std::int32_t> size) override {
        std::memset(buffer, _color, 4 * size.first * size.second);
        
        _color += _ascending ? 1 : -1;
        if ((_ascending && _color == std::numeric_limits<decltype(_color)>::max())
        || (!_ascending && _color == std::numeric_limits<decltype(_color)>::min())) {
            _ascending = !_ascending;
        }
    }
private:
    std::uint8_t _color = 0;
    bool _ascending = true;
};

int main() {
    Display display;
    SimpleWindow window(display, WINDOW_TITLE);

    while (!window.should_close()) {
        display.poll_events();
    }
}
