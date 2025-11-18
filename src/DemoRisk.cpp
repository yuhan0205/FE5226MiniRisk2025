#include <iostream>
#include <algorithm>
#include <set>
#include <fstream>

#include "Macros.h"
#include "MarketDataServer.h"
#include "PortfolioUtils.h"
#include "FixingDataServer.h"
#include "TradePayment.h"

using namespace::minirisk;

// Helper function to check if a file exists
static bool file_exists(const string& filename)
{
    std::ifstream file(filename);
    return file.good();
}

void run(const string& portfolio_file, const string& risk_factors_file, const string& base_ccy, const string& fixings_file)
{
    // Validate file existence
    MYASSERT(file_exists(portfolio_file), "Portfolio file does not exist: " << portfolio_file);
    MYASSERT(file_exists(risk_factors_file), "Risk factors file does not exist: " << risk_factors_file);
    if (!fixings_file.empty()) {
        MYASSERT(file_exists(fixings_file), "Fixings file does not exist: " << fixings_file);
    }
    
    // Validate base currency is not empty
    MYASSERT(!base_ccy.empty(), "Base currency cannot be empty");
    MYASSERT(base_ccy.length() == 3, "Base currency must be 3 characters (ISO 4217 code), got: " << base_ccy);
    
    // load the portfolio from file
    portfolio_t portfolio = load_portfolio(portfolio_file);

    // save and reload portfolio to implicitly test round trip serialization
    save_portfolio("portfolio.tmp", portfolio);
    portfolio.clear();
    portfolio = load_portfolio("portfolio.tmp");

    // display portfolio
    print_portfolio(portfolio);

    // get pricers configured with base currency
    std::vector<ppricer_t> pricers(get_pricers(portfolio, base_ccy));

    // initialize market data server
    std::shared_ptr<const MarketDataServer> mds(new MarketDataServer(risk_factors_file));

    // initialize fixing data server (optional)
    std::unique_ptr<FixingDataServer> fds;
    if (!fixings_file.empty()) {
        fds.reset(new FixingDataServer(fixings_file));
    }

    // Init market object
    Date today(2017,8,5);
    Market mkt(mds, today);

    // Price all products. Market objects are automatically constructed on demand,
    // fetching data as needed from the market data server.
    {
        auto prices = compute_prices(pricers, mkt, fds.get());
        print_price_vector("PV", prices);
    }

    // Preload all risk factors before any pricing calculations
    // This ensures all risk factors are cached in the market object
    {
        std::cout << "Risk factors:\n";
        // Determine currencies involved
        std::set<string> trade_ccys;
        for (const auto& t : portfolio) {
            const TradePayment* tp = dynamic_cast<const TradePayment*>(t.get());
            if (tp) trade_ccys.insert(tp->ccy());
        }
        std::set<string> fx_ccys = trade_ccys;
        fx_ccys.insert(base_ccy);
        // If cross conversion is needed for any trade (neither side is USD), include USD
        bool needs_usd = (base_ccy != "USD");
        if (needs_usd) {
            for (const auto& c : trade_ccys) {
                if (c != "USD" && c != base_ccy) { needs_usd = true; break; }
                needs_usd = false;
            }
        }
        if (needs_usd) fx_ccys.insert("USD");

        // Load all risk factors from the market data server
        auto all_risk_factors = mds->match(".+");
        for (const auto& rf : all_risk_factors) {
            // Access each risk factor to trigger loading into market cache
            mkt.get_value(rf, "risk factor");
            // FX spot risk factors for relevant currencies
            if (rf.find(fx_spot_prefix) == 0) {
                string ccy = rf.substr(fx_spot_prefix.size());
                if (fx_ccys.count(ccy))
                    std::cout << rf << "\n";
                continue;
            }
            // IR tenor risk factors for portfolio currencies
            if (rf.find(ir_rate_prefix) == 0) {
                if (rf.size() >= 3) {
                    string ccy = rf.substr(rf.size() - 3, 3);
                    if (trade_ccys.count(ccy))
                        std::cout << rf << "\n";
                }
            }
        }
        std::cout << "\n";
    }

    {   // Compute PV01 Bucketed (i.e. sensitivity with respect to individual yield curve points)
        std::vector<std::pair<string, portfolio_values_t>> pv01_bucketed(compute_pv01_bucketed(pricers,mkt,fds.get()));

        // display PV01 Bucketed per tenor
        for (const auto& g : pv01_bucketed)
            print_price_vector("PV01 bucketed " + g.first, g.second);
    }

    {   // Compute PV01 Parallel (i.e. sensitivity with respect to parallel shift of yield curves)
        std::vector<std::pair<string, portfolio_values_t>> pv01_parallel(compute_pv01_parallel(pricers,mkt,fds.get()));

        // display PV01 Parallel per currency
        for (const auto& g : pv01_parallel)
            print_price_vector("PV01 parallel " + g.first, g.second);
    }

    {   // Compute FX delta (sensitivity wrt FX spot quoted against USD)
        std::vector<std::pair<string, portfolio_values_t>> fx_delta(compute_fx_delta(pricers, mkt, fds.get()));

        // Determine relevant FX currencies from portfolio and base currency
        std::set<string> trade_ccys;
        for (const auto& t : portfolio) {
            const TradePayment* tp = dynamic_cast<const TradePayment*>(t.get());
            if (tp) trade_ccys.insert(tp->ccy());
        }
        std::set<string> fx_ccys = trade_ccys;
        fx_ccys.insert(base_ccy);
        bool needs_usd = (base_ccy != "USD");
        if (needs_usd) {
            for (const auto& c : trade_ccys) {
                if (c != "USD" && c != base_ccy) { needs_usd = true; break; }
                needs_usd = false;
            }
        }
        if (needs_usd) fx_ccys.insert("USD");

        // display FX delta only for relevant currencies
        for (const auto& g : fx_delta) {
            // g.first is like "FX.SPOT.CCY"
            const string prefix = fx_spot_prefix; // e.g. "FX.SPOT."
            string ccy = (g.first.size() > prefix.size()) ? g.first.substr(prefix.size()) : g.first;
            if (fx_ccys.count(ccy))
                print_price_vector("FX delta " + g.first, g.second);
        }
    }

    // disconnect the market (no more fetching from the market data server allowed)
    mkt.disconnect();
}

void usage(const char* program_name)
{
    std::cerr
        << "Usage: " << program_name << " -p <portfolio_file> -f <risk_factors_file> [-b <base_currency>] [-x <fixings_file>]\n"
        << "\n"
        << "Required arguments:\n"
        << "  -p <portfolio_file>        Path to the portfolio file\n"
        << "  -f <risk_factors_file>      Path to the risk factors file\n"
        << "\n"
        << "Optional arguments:\n"
        << "  -b <base_currency>         Base currency (default: USD)\n"
        << "  -x <fixings_file>          Path to the fixings file (optional)\n"
        << "\n"
        << "Examples:\n"
        << "  " << program_name << " -p data/portfolio_00.txt -f data/risk_factors_0.txt\n"
        << "  " << program_name << " -p data/portfolio_04.txt -f data/risk_factors_3.txt -b GBP\n"
        << "  " << program_name << " -p data/portfolio_04.txt -f data/risk_factors_3.txt -b GBP -x data/fixings.txt\n";
    std::exit(1);
}

int main(int argc, const char **argv)
{
    // Handle no arguments case
    if (argc == 1) {
        usage(argv[0]);
    }
    
    // parse command line arguments
    string portfolio, riskfactors;
    string base_ccy = "USD";
    string fixings_file;
    
    // Validate argument count (must be odd: program name + pairs of key-value)
    if (argc < 5 || argc % 2 == 0) {
        std::cerr << "Error: Invalid number of arguments.\n\n";
        usage(argv[0]);
    }
    
    // Track which required arguments we've seen
    bool has_portfolio = false;
    bool has_riskfactors = false;
    
    for (int i = 1; i < argc; i += 2) {
        // Check if we have enough arguments remaining
        if (i + 1 >= argc) {
            std::cerr << "Error: Missing value for argument: " << argv[i] << "\n\n";
            usage(argv[0]);
        }
        
        string key(argv[i]);
        string value(argv[i+1]);
        
        // Validate that key starts with '-'
        if (key.empty() || key[0] != '-') {
            std::cerr << "Error: Invalid argument format: " << key << " (arguments must start with '-')\n\n";
            usage(argv[0]);
        }
        
        // Validate that value is not empty
        if (value.empty()) {
            std::cerr << "Error: Empty value provided for argument: " << key << "\n\n";
            usage(argv[0]);
        }
        
        if (key == "-p") {
            portfolio = value;
            has_portfolio = true;
        } else if (key == "-f") {
            riskfactors = value;
            has_riskfactors = true;
        } else if (key == "-b") {
            base_ccy = value;
        } else if (key == "-x") {
            fixings_file = value;
        } else {
            std::cerr << "Error: Unknown argument: " << key << "\n\n";
            usage(argv[0]);
        }
    }
    
    // Validate required arguments are present
    if (!has_portfolio || !has_riskfactors) {
        std::cerr << "Error: Missing required arguments.\n\n";
        usage(argv[0]);
    }

    try {
        run(portfolio, riskfactors, base_ccy, fixings_file);
        return 0;  // report success to the caller
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return -1; // report an error to the caller
    }
    catch (...)
    {
        std::cerr << "Unknown exception occurred\n";
        return -1; // report an error to the caller
    }
}

// Under src folder: make
// src/bin/DemoRisk.exe -p data/portfolio_00.txt -f data/risk_factors_0.txt
// src/bin/DemoRisk.exe -p data/portfolio_01.txt -f data/risk_factors_3.txt
// src/bin/DemoRisk.exe -p data/portfolio_04.txt -f data/risk_factors_3.txt
// src/bin/DemoRisk.exe -p data/portfolio_04.txt -f data/risk_factors_3.txt -b GBP
// src/bin/DemoRisk.exe -p data/portfolio_04.txt -f data/risk_factors_3.txt -b GBP -x data/fixings.txt
// src/bin/DemoRisk.exe -p data/portfolio_04.txt -f data/risk_factors_3.txt -x data/fixings.txt
// src/bin/DemoRisk.exe -p data/portfolio_10.txt -f data/risk_factors_3.txt -x data/fixings.txt
// src/bin/DemoRisk.exe -p data/portfolio_10.txt -f data/risk_factors_3.txt -b GBP -x data/fixings.txt
