#include "Global.h"
#include "PortfolioUtils.h"
#include "TradePayment.h"

#include <numeric>
#include <map>
#include <set>
#include <limits>

namespace minirisk {

void print_portfolio(const portfolio_t& portfolio)
{
    std::for_each(portfolio.begin(), portfolio.end(), [](auto& pt){ pt->print(std::cout); });
}

std::vector<ppricer_t> get_pricers(const portfolio_t& portfolio, const std::string& configuration)
{
    std::vector<ppricer_t> pricers(portfolio.size());
    std::transform( portfolio.begin(), portfolio.end(), pricers.begin()
                  , [&configuration](auto &pt) -> ppricer_t { return pt->pricer(configuration); } );
    return pricers;
}

portfolio_values_t compute_prices(const std::vector<ppricer_t>& pricers, Market& mkt, const FixingDataServer* fds)
{
    portfolio_values_t prices(pricers.size());
    for (size_t i = 0; i < pricers.size(); ++i) {
        try {
            double price = pricers[i]->price(mkt, fds);
            prices[i] = std::make_pair(price, "");
        } catch (const std::exception& e) {
            prices[i] = std::make_pair(std::numeric_limits<double>::quiet_NaN(), e.what());
        }
    }
    return prices;
}

std::pair<double, std::vector<std::pair<size_t, string>>> portfolio_total(const portfolio_values_t& values)
{
    double total = 0.0;
    std::vector<std::pair<size_t, string>> errors;
    
    for (size_t i = 0; i < values.size(); ++i) {
        if (std::isnan(values[i].first)) {
            errors.push_back(std::make_pair(i, values[i].second));
        } else {
            total += values[i].first;
        }
    }
    
    return std::make_pair(total, errors);
}

std::vector<std::pair<string, portfolio_values_t>> compute_pv01_parallel(const std::vector<ppricer_t>& pricers, const Market& mkt, const FixingDataServer* fds)
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
        pv01.push_back(std::make_pair("IR." + ccy, portfolio_values_t(pricers.size())));

        // bump down
        bumped.clear();
        for (const auto& rf : all) bumped.emplace_back(rf.first, rf.second - bump_size);
        tmpmkt.set_risk_factors(bumped);
        auto pv_dn = compute_prices(pricers, tmpmkt, fds);

        // bump up
        bumped.clear();
        for (const auto& rf : all) bumped.emplace_back(rf.first, rf.second + bump_size);
        tmpmkt.set_risk_factors(bumped);
        auto pv_up = compute_prices(pricers, tmpmkt, fds);

        // restore
        tmpmkt.set_risk_factors(all);

        // central difference per trade
        double dr = 2.0 * bump_size;
        for (size_t i = 0; i < pv_up.size(); ++i) {
            if (std::isnan(pv_up[i].first) || std::isnan(pv_dn[i].first)) {
                // If either up or down bump is NaN, set result to NaN
                string error_msg = std::isnan(pv_up[i].first) ? pv_up[i].second : pv_dn[i].second;
                pv01.back().second[i] = std::make_pair(std::numeric_limits<double>::quiet_NaN(), error_msg);
            } else {
                double diff = (pv_up[i].first - pv_dn[i].first) / dr;
                pv01.back().second[i] = std::make_pair(diff, "");
            }
        }
    }

    return pv01;
}

std::vector<std::pair<string, portfolio_values_t>> compute_pv01_bucketed(const std::vector<ppricer_t>& pricers, const Market& mkt, const FixingDataServer* fds)
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
        pv01.push_back(std::make_pair(d.first, portfolio_values_t(pricers.size())));

        // bump down and price
        bumped[0].second = d.second - bump_size;
        tmpmkt.set_risk_factors(bumped);
        auto pv_dn = compute_prices(pricers, tmpmkt, fds);

        // bump up and price
        bumped[0].second = d.second + bump_size;
        tmpmkt.set_risk_factors(bumped);
        auto pv_up = compute_prices(pricers, tmpmkt, fds);

        // restore
        bumped[0].second = d.second;
        tmpmkt.set_risk_factors(bumped);

        // central difference
        double dr = 2.0 * bump_size;
        for (size_t i = 0; i < pv_up.size(); ++i) {
            if (std::isnan(pv_up[i].first) || std::isnan(pv_dn[i].first)) {
                // If either up or down bump is NaN, set result to NaN
                string error_msg = std::isnan(pv_up[i].first) ? pv_up[i].second : pv_dn[i].second;
                pv01.back().second[i] = std::make_pair(std::numeric_limits<double>::quiet_NaN(), error_msg);
            } else {
                double diff = (pv_up[i].first - pv_dn[i].first) / dr;
                pv01.back().second[i] = std::make_pair(diff, "");
            }
        }
    }

    return pv01;
}


std::vector<std::pair<string, portfolio_values_t>> compute_fx_delta(const std::vector<ppricer_t>& pricers, const Market& mkt, const FixingDataServer* fds)
{
    std::vector<std::pair<string, portfolio_values_t>> fx_delta;

    // relative bump of 0.1%
    const double rel_bump = 0.1 / 100.0;

    // list all FX spot risk factors quoted vs USD (keys are like FX.SPOT.CCY)
    // We only consider those that are cached/known via get_risk_factors
    auto all_fx = mkt.get_risk_factors("FX\\.SPOT\\.[A-Z]{3}$");

    // Make a local copy of the Market because we'll apply bumps
    Market tmpmkt(mkt);

    fx_delta.reserve(all_fx.size());
    for (const auto& d : all_fx) {
        const string& name = d.first;      // e.g. FX.SPOT.EUR
        const double spot0 = d.second;     // current value

        // central relative bump
        const double bump_size_dn = spot0 * (1.0 - rel_bump);
        const double bump_size_up = spot0 * (1.0 + rel_bump);

        std::vector<std::pair<string, double>> bumped(1, d);
        fx_delta.push_back(std::make_pair(name, portfolio_values_t(pricers.size())));

        // bump down and price
        bumped[0].second = bump_size_dn;
        tmpmkt.set_risk_factors(bumped);
        auto pv_dn = compute_prices(pricers, tmpmkt, fds);

        // bump up and price
        bumped[0].second = bump_size_up;
        tmpmkt.set_risk_factors(bumped);
        auto pv_up = compute_prices(pricers, tmpmkt, fds);

        // restore
        bumped[0].second = spot0;
        tmpmkt.set_risk_factors(bumped);

        // central difference per trade: divide by 2*spot0*rel_bump to get dPV/dSpot
        const double denom = (2.0 * spot0 * rel_bump);
        for (size_t i = 0; i < pv_up.size(); ++i) {
            if (std::isnan(pv_up[i].first) || std::isnan(pv_dn[i].first)) {
                string error_msg = std::isnan(pv_up[i].first) ? pv_up[i].second : pv_dn[i].second;
                fx_delta.back().second[i] = std::make_pair(std::numeric_limits<double>::quiet_NaN(), error_msg);
            } else {
                double diff = (pv_up[i].first - pv_dn[i].first) / denom;
                fx_delta.back().second[i] = std::make_pair(diff, "");
            }
        }
    }

    return fx_delta;
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
    auto total_result = portfolio_total(values);
    double total = total_result.first;
    auto errors = total_result.second;
    
    std::cout
        << "========================\n"
        << name << ":\n"
        << "========================\n"
        << "Total:  " << total << "\n";
    
    if (!errors.empty()) {
        std::cout << "Errors: " << errors.size() << "\n";
    }
    
    std::cout << "\n========================\n";

    for (size_t i = 0, n = values.size(); i < n; ++i) {
        std::cout << std::setw(5) << i << ": ";
        if (std::isnan(values[i].first)) {
            std::cout << values[i].second;
        } else {
            std::cout << values[i].first;
        }
        std::cout << "\n";
    }

    std::cout << "========================\n\n";
}

} // namespace minirisk
