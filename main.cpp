import vulkan;
import wayland;
import xkb;

#include <xkbcommon/xkbcommon.h>

#include <cstdint>
#include <cstdio>
#include <string_view>
#include <utility>

static constexpr char WINDOW_TITLE[] = "Hello Wayland";

class SimpleWindow final : public Window {
public:
    SimpleWindow(Display& display, const char *title)
        :Window(display, title),
        _renderer(display.handle(), handle()),
        _color(0), _ascending(true)
    {

    }

    void configure(std::pair<std::int32_t, std::int32_t> size) override {
        _renderer.resize({size.first, size.second});
    }

    void key_down(xkb_keysym_t key, bool alt, bool ctrl, bool shift) override {
        if (key == XKB_KEY_Return && alt) {
            toggle_fullscreen();
        }
    }
    void key_up(xkb_keysym_t key, bool alt, bool ctrl, bool shift) override {

    }
    void pointer_click(std::int32_t x, std::int32_t y) override {
        
    }
    void pointer_release(std::int32_t x, std::int32_t y) override {
        
    }
    void render() override {
        _color = _color + (_ascending ? 1 : -1);
        if (_ascending && _color == UINT8_MAX || !_ascending && !_color)
        {
            _ascending = !_ascending;
        }
        _renderer.render(_color);
    }
    void text(std::string_view string) override {
        
    }

private:
    Renderer _renderer;
    std::uint8_t _color;
    bool _ascending;
};

int main() {
    Display display;
    SimpleWindow window(display, WINDOW_TITLE);

    while (!window.should_close()) {
        display.poll_events();
    }
}
