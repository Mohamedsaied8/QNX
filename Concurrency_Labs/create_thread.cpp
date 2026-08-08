#include <thread>
#include <iostream>
#include <chrono>
#include <fstream>
#include <ostream>

void OutputFileThread()
{
    while(1)
    {
        std::ofstream file("/home/mohamed/Desktop/CppConcurrency/Labs/testing.txt");
        file << "write data";
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}
void InputTofileThread()
{
    while(1)
    {
        //get data from somewhere
        std::string buffer;
        std::ifstream f("/home/mohamed/Desktop/CppConcurrency/Labs/testing.txt");
        f >> buffer;
        unsigned int n = std::thread::hardware_concurrency();
        std::cout << "number of threads supported by hardware : " << n <<  std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    }
}


int main()
{
    std::thread thread1(InputTofileThread);
    std::thread thread2(OutputFileThread);

    thread1.join();
    thread2.join();

    return 0;
}