import wayland;
import std;

static constexpr char WINDOW_TITLE[] = "Hello Wayland";

int main() {
    Display display;
    Window window(display, WINDOW_TITLE);

    bool ascending = true;
    std::uint8_t color = 0x00;
    while (!window.should_close()) {
        display.poll_events();
        window.render(color);

        color += ascending ? 1 : -1;
        if ((ascending && color == std::numeric_limits<std::uint8_t>::max())
        || (!ascending && color == std::numeric_limits<std::uint8_t>::min())) {
            ascending = !ascending;
        }
    }
}
