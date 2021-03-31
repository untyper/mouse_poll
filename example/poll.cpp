#include <iostream>
#include <mouse_poll.hpp>

void on_mouse_input(mouse_poll* mp) {
    std::cout << "x: "               << mp->input.delta_x
              << ", y: "             << mp->input.delta_y
              << ", wheel: "         << mp->input.delta_wheel
              << ", left_button: "   << mp->input.button1
              << ", right_button: "  << mp->input.button2
              << ", middle_button: " << mp->input.button3
              << ", xbutton_1: "     << mp->input.button4
              << ", xbutton_2: "     << mp->input.button5
              << "\n";
}

int main() {
    mouse_poll mp(on_mouse_input);
    mp.start();
    std::cout << "[+] Running..." << std::endl;
    // Press enter to exit
    std::cin.get();
    mp.stop();
    std::cout << "[-] Stopped." << std::endl;
    return 0;
}