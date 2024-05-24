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

    void configure(std::pair<std::int32_t, std::int32_t> size) override {
        std::printf("%dx%d\n", size.first, size.second);
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
        
    }
    void pointer_release(std::int32_t x, std::int32_t y) override {
        
    }
    void render() override {
        std::putchar('.');
    }
    void text(std::string_view string) override {
        
    }
};

int main() {
    Display display;
    SimpleWindow window(display, WINDOW_TITLE);

    while (!window.should_close()) {
        display.poll_events();
    }
}
