#include "src/Date.h"
#include "src/Streamer.h"
#include <iostream>

int main() {
    using namespace minirisk;
    
    // Test serial date parsing
    std::cout << "Testing serial date parsing:" << std::endl;
    
    // Test the serial dates from the portfolio
    Date date1(43860);
    Date date2(43861);
    
    std::cout << "Serial 43860 -> " << date1.to_string(true) << std::endl;
    std::cout << "Serial 43861 -> " << date2.to_string(true) << std::endl;
    
    // Test the date difference calculation
    Date today(2017, 8, 5);
    std::cout << "Today: " << today.to_string(true) << std::endl;
    std::cout << "Days from today to date1: " << (date1 - today) << std::endl;
    std::cout << "Days from today to date2: " << (date2 - today) << std::endl;
    
    return 0;
}
