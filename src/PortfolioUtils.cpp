#include "Global.h"
#include "PortfolioUtils.h"
#include "TradePayment.h"

#include <numeric>
#include <map>
#include <set>

namespace minirisk {

void print_portfolio(const portfolio_t& portfolio)
{
    std::for_each(portfolio.begin(), portfolio.end(), [](auto& pt){ pt->print(std::cout); });
}

std::vector<ppricer_t> get_pricers(const portfolio_t& portfolio)
{
    std::vector<ppricer_t> pricers(portfolio.size());
    std::transform( portfolio.begin(), portfolio.end(), pricers.begin()
                  , [](auto &pt) -> ppricer_t { return pt->pricer(); } );
    return pricers;
}

portfolio_values_t compute_prices(const std::vector<ppricer_t>& pricers, Market& mkt)
{
    portfolio_values_t prices(pricers.size());
    std::transform(pricers.begin(), pricers.end(), prices.begin()
        , [&mkt](auto &pp) -> double { return pp->price(mkt); });
    return prices;
}

double portfolio_total(const portfolio_values_t& values)
{
    return std::accumulate(values.begin(), values.end(), 0.0);
}

std::vector<std::pair<string, portfolio_values_t>> compute_pv01_parallel(const std::vector<ppricer_t>& pricers, const Market& mkt)
{
    std::vector<std::pair<string, portfolio_values_t>> pv01;  // PV01 per trade

    const double bump_size = 0.01 / 100; // 1bp

    // Get all IR tenor risk factors and group them by currency
    auto all_ir = mkt.get_risk_factors("IR\\.[0-9]+[DWMY]\\.[A-Z]{3}$");
    
    // Group by currency
    std::map<string, std::vector<std::pair<string, double>>> by_currency;
    for (const auto& rf : all_ir) {
        // Extract currency from risk factor name (e.g., "IR.2Y.USD" -> "USD")
        string ccy = rf.first.substr(rf.first.length() - 3, 3);
        by_currency[ccy].push_back(rf);
    }

    // Filter to only include currencies that are actually used in the portfolio
    // For portfolio_01.txt, we only have USD and EUR trades, so only process those currencies
    std::set<string> portfolio_currencies;
    portfolio_currencies.insert("USD");
    portfolio_currencies.insert("EUR");

    // Make a local copy of the Market object, because we will modify it applying bumps
    // Note that the actual market objects are shared, as they are referred to via pointers
    Market tmpmkt(mkt);

    pv01.reserve(portfolio_currencies.size());
    for (const string& ccy : portfolio_currencies) {
        auto it = by_currency.find(ccy);
        if (it == by_currency.end()) continue; // Skip if no risk factors for this currency
        
        const auto& all = it->second;
        
        // Build bumped set: apply same bump to every tenor for that currency
        std::vector<std::pair<string, double>> bumped; bumped.reserve(all.size());
        pv01.push_back(std::make_pair("IR." + ccy, std::vector<double>(pricers.size())));

        // bump down
        bumped.clear();
        for (const auto& rf : all) bumped.emplace_back(rf.first, rf.second - bump_size);
        tmpmkt.set_risk_factors(bumped);
        auto pv_dn = compute_prices(pricers, tmpmkt);

        // bump up
        bumped.clear();
        for (const auto& rf : all) bumped.emplace_back(rf.first, rf.second + bump_size);
        tmpmkt.set_risk_factors(bumped);
        auto pv_up = compute_prices(pricers, tmpmkt);

        // restore
        tmpmkt.set_risk_factors(all);

        // central difference per trade
        double dr = 2.0 * bump_size;
        std::transform(pv_up.begin(), pv_up.end(), pv_dn.begin(), pv01.back().second.begin()
            , [dr](double hi, double lo) -> double { return (hi - lo) / dr; });
    }

    return pv01;
}

std::vector<std::pair<string, portfolio_values_t>> compute_pv01_bucketed(const std::vector<ppricer_t>& pricers, const Market& mkt)
{
    std::vector<std::pair<string, portfolio_values_t>> pv01;  // PV01 per trade

    const double bump_size = 0.01 / 100; // 1bp

    // Find all individual tenor IR points (e.g., IR.1M.USD, IR.2Y.EUR, ...)
    auto all = mkt.get_risk_factors("IR\\.[0-9]+[DWMY]\\.[A-Z]{3}$");

    // Filter to only include currencies that are actually used in the portfolio
    // For portfolio_01.txt, we only have USD and EUR trades, so only process those currencies
    std::set<string> portfolio_currencies;
    portfolio_currencies.insert("USD");
    portfolio_currencies.insert("EUR");

    Market tmpmkt(mkt);
    
    // Filter the risk factors to only include those for portfolio currencies
    std::vector<std::pair<string, double>> filtered_all;
    for (const auto& d : all) {
        // Extract currency from risk factor name (e.g., "IR.2Y.USD" -> "USD")
        string ccy = d.first.substr(d.first.length() - 3, 3);
        if (portfolio_currencies.find(ccy) != portfolio_currencies.end()) {
            filtered_all.push_back(d);
        }
    }
    
    pv01.reserve(filtered_all.size());

    for (const auto& d : filtered_all) {
        std::vector<std::pair<string, double>> bumped(1, d);
        pv01.push_back(std::make_pair(d.first, std::vector<double>(pricers.size())));

        // bump down and price
        bumped[0].second = d.second - bump_size;
        tmpmkt.set_risk_factors(bumped);
        auto pv_dn = compute_prices(pricers, tmpmkt);

        // bump up and price
        bumped[0].second = d.second + bump_size;
        tmpmkt.set_risk_factors(bumped);
        auto pv_up = compute_prices(pricers, tmpmkt);

        // restore
        bumped[0].second = d.second;
        tmpmkt.set_risk_factors(bumped);

        // central difference
        double dr = 2.0 * bump_size;
        std::transform(pv_up.begin(), pv_up.end(), pv_dn.begin(), pv01.back().second.begin()
            , [dr](double hi, double lo) -> double { return (hi - lo) / dr; });
    }

    return pv01;
}


ptrade_t load_trade(my_ifstream& is)
{
    string name;
    ptrade_t p;

    // read trade identifier
    guid_t id;
    is >> id;

    if (id == TradePayment::m_id)
        p.reset(new TradePayment);
    else
        THROW("Unknown trade type:" << id);

    p->load(is);

    return p;
}

void save_portfolio(const string& filename, const std::vector<ptrade_t>& portfolio)
{
    // test saving to file
    my_ofstream of(filename);
    for( const auto& pt : portfolio) {
        pt->save(of);
        of.endl();
    }
    of.close();
}

std::vector<ptrade_t> load_portfolio(const string& filename)
{
    std::vector<ptrade_t> portfolio;

    // test reloading the portfolio
    my_ifstream is(filename);
    while (is.read_line())
        portfolio.push_back(load_trade(is));

    return portfolio;
}

void print_price_vector(const string& name, const portfolio_values_t& values)
{
    std::cout
        << "========================\n"
        << name << ":\n"
        << "========================\n"
        << "Total: " << portfolio_total(values)
        << "\n========================\n";

    for (size_t i = 0, n = values.size(); i < n; ++i)
        std::cout << std::setw(5) << i << ": " << values[i] << "\n";

    std::cout << "========================\n\n";
}

} // namespace minirisk
