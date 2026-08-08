// Lab 01 — Creating threads  (SOLUTION)
//
// Both tasks now run on their own thread, so the LCD refresh and the LED blink
// happen concurrently. Their output interleaves instead of running one after
// the other.

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace std::chrono_literals;

void lcd_task(const std::string& label, int reading)
{
    for (int i = 0; i < 5; ++i) {
        std::cout << "[LCD] " << label << reading << "\n";
        std::this_thread::sleep_for(500ms);
    }
}

void led_task()
{
    for (int i = 0; i < 5; ++i) {
        std::cout << "[LED] " << (i % 2 ? "off" : "ON ") << "\n";
        std::this_thread::sleep_for(500ms);
    }
}

int main()
{
    std::string label = "Temperature: ";
    int temperature = 43;

    // Each std::thread starts running the moment it is constructed.
    std::thread lcd(lcd_task, label, temperature);
    std::thread led(led_task);

    // join() blocks until that thread finishes. A std::thread that is still
    // joinable when its destructor runs calls std::terminate() — so ALWAYS
    // join() (or detach(), see Lab 03) before the thread object dies.
    lcd.join();
    led.join();

    std::cout << "Both tasks complete.\n";
    return 0;
}
