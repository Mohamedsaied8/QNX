// Lab 01 — Creating threads  (STARTER)
//
// An embedded device must refresh an LCD *and* blink a status LED at the same
// time. Right now both run on the main thread, so the LED only blinks AFTER the
// LCD loop finishes — which it never does. The device feels frozen.
//
// TASK:
//   1. Run lcd_task() on its own std::thread.
//   2. Run led_task() on its own std::thread.
//   3. join() both before main returns.
//   Observe that the two outputs now interleave.
//
// Hint: std::thread t(function, arg1, arg2, ...);  then  t.join();

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

    // TODO: launch lcd_task(label, temperature) and led_task() concurrently,
    //       then join both. For now they run one after another (serially):
    lcd_task(label, temperature);
    led_task();

    return 0;
}
