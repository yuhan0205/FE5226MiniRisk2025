#include "src/Date.h"
#include "src/MarketDataServer.h"
#include "src/Market.h"
#include "src/CurveDiscount.h"
#include <iostream>

int main() {
    using namespace minirisk;
    
    // Create market data server
    std::shared_ptr<const MarketDataServer> mds(new MarketDataServer("data/risk_factors_3.txt"));
    
    // Create market object
    Date today(2017, 8, 5);
    Market mkt(mds, today);
    
    // Test discount curve construction for USD
    std::cout << "Testing discount curve construction for USD:" << std::endl;
    
    try {
        auto disc_curve = mkt.get_discount_curve("IR.DISCOUNT.USD");
        std::cout << "Discount curve created successfully" << std::endl;
        
        // Test discount factor calculation
        Date test_date(2020, 1, 30);
        double df = disc_curve->df(test_date);
        std::cout << "Discount factor for " << test_date.to_string(true) << ": " << df << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error creating discount curve: " << e.what() << std::endl;
    }
    
    return 0;
}
