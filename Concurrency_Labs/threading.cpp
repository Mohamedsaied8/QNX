#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
float temperature = 0.0;
std::mutex m;
void TemperatureSensor()
{
    temperature = 25.0; //location RAM
    while(1)
    {
        m.lock();
        //read temperature
        temperature += 0.5;
        /*
          mov R1, [temperature] <-- read
          add R1, 0.5           <-- Modify
          STR R1, [temperature] <-- write
        */
        m.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void Display()
{
    while(1)
    {
        m.lock();
        std::cout << "Temperature is "<< temperature <<" degrees\n";
        m.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
}

struct Accelerometer
{
    void operator()()
    {
        while (1)
        {
            std::cout << "Accelerometer task\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
    }
};

int main()
{
    std::thread sensor_thread(TemperatureSensor);
    std::thread display_thread(Display);

    //Accelerometer acc;
    // std::thread accel_thread([](){
    //     while(1)
    //     {
    //         std::cout << "Accelerometer task\n";
    //         std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    //     }
    // });
 
    sensor_thread.join();
    display_thread.join();
   // accel_thread.join();
}