#pragma once

#include <vector>

#include "ITrade.h"
#include "IPricer.h"

namespace minirisk {

struct Market;
struct FixingDataServer;

typedef std::vector<std::pair<double, string>> portfolio_values_t;

// get pricer for each trade with configuration (e.g., base currency)
std::vector<ppricer_t> get_pricers(const portfolio_t& portfolio, const std::string& configuration);

// compute prices
portfolio_values_t compute_prices(const std::vector<ppricer_t>& pricers, Market& mkt, const FixingDataServer* fds);

// compute the cumulative book value
std::pair<double, std::vector<std::pair<size_t, string>>> portfolio_total(const portfolio_values_t& values);

// Compute PV01 Parallel: sensitivity to parallel shift of the yield curve per currency
// Use central differences, absolute bump of 0.01%
std::vector<std::pair<string, portfolio_values_t>> compute_pv01_parallel(const std::vector<ppricer_t>& pricers, const Market& mkt, const FixingDataServer* fds);

// Compute PV01 Bucketed: sensitivity to each individual yield curve point (tenor)
// Use central differences, absolute bump of 0.01%
std::vector<std::pair<string, portfolio_values_t>> compute_pv01_bucketed(const std::vector<ppricer_t>& pricers, const Market& mkt, const FixingDataServer* fds);

// Compute FX Delta: sensitivity to FX spot rates quoted against USD
// Use central differences, relative bump of 0.1%
std::vector<std::pair<string, portfolio_values_t>> compute_fx_delta(const std::vector<ppricer_t>& pricers, const Market& mkt, const FixingDataServer* fds);

// save portfolio to file
void save_portfolio(const string& filename, const std::vector<ptrade_t>& portfolio);

// load portfolio from file
std::vector<ptrade_t>  load_portfolio(const string& filename);

// print portfolio to cout
void print_portfolio(const portfolio_t& portfolio);

// print portfolio to cout
void print_price_vector(const string& name, const portfolio_values_t& values);


} // namespace minirisk

