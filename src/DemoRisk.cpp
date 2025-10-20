#include <iostream>
#include <algorithm>
#include <set>

#include "MarketDataServer.h"
#include "PortfolioUtils.h"
#include "FixingDataServer.h"
#include "TradePayment.h"

using namespace::minirisk;

void run(const string& portfolio_file, const string& risk_factors_file, const string& base_ccy, const string& fixings_file)
{
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

void usage()
{
    std::cerr
        << "Invalid command line arguments\n"
        << "Example:\n"
        << "DemoRisk -p portfolio.txt -f risk_factors.txt [-b CCY] [-x fixings.txt]\n";
    std::exit(-1);
}

int main(int argc, const char **argv)
{
    // parse command line arguments
    string portfolio, riskfactors;
    string base_ccy = "USD";
    string fixings_file;
    if (argc < 5 || argc % 2 == 0)
        usage();
    for (int i = 1; i < argc; i += 2) {
        string key(argv[i]);
        string value(argv[i+1]);
        if (key == "-p")
            portfolio = value;
        else if (key == "-f")
            riskfactors = value;
        else if (key == "-b")
            base_ccy = value;
        else if (key == "-x")
            fixings_file = value;
        else
            usage();
    }
    if (portfolio == "" || riskfactors == "")
        usage();

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