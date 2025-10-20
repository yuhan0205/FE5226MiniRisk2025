#include "CurveDiscount.h"
#include "Market.h"
#include "Streamer.h"

#include <cmath>
#include <regex>
#include <algorithm>


namespace minirisk {

static unsigned tenor_to_days(unsigned n, char unit)
{
    switch (unit) {
    case 'D': return n;
    case 'W': return n * 7u;
    case 'M': return n * 30u;
    case 'Y': return n * 365u;
    default:
        MYASSERT(false, "Unsupported tenor unit: " << unit);
        return 0;
    }
}

CurveDiscount::CurveDiscount(Market *mkt, const Date& today, const string& curve_name)
    : m_today(today)
    , m_name(curve_name)
{
    string ccy = curve_name.substr(ir_curve_discount_prefix.length(), 3);

    std::regex pattern(std::string("^IR\\.([0-9]+)([DWMY])\\.") + ccy + "$");
    auto keys = mkt->match_keys(std::string("^IR\\.[0-9]+[DWMY]\\.") + ccy + "$" );

    std::vector<std::pair<unsigned,double>> grid;
    grid.reserve(keys.size());

    for (const auto& key : keys) {
        std::smatch m;
        if (std::regex_match(key, m, pattern)) {
            unsigned n = static_cast<unsigned>(std::stoul(m[1].str()));
            char unit = m[2].str()[0];
            unsigned days = tenor_to_days(n, unit);
            double r = mkt->get_value(key, "yield");
            grid.emplace_back(days, r);
        }
    }

    MYASSERT(!grid.empty(), "No tenor points found for curve " << curve_name);
    std::sort(grid.begin(), grid.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
    grid.erase(std::unique(grid.begin(), grid.end(), [](const auto& a, const auto& b){ return a.first == b.first; }), grid.end());

    const size_t n = grid.size();
    m_T.clear(); m_r.clear(); m_rT_prefix.clear(); m_r_local.clear();
    m_T.reserve(n + 1);
    m_r.reserve(n + 1);
    m_rT_prefix.reserve(n + 1);

    m_T.push_back(0u);
    m_r.push_back(grid.front().second);
    m_rT_prefix.push_back(0.0);

    for (const auto& p : grid) {
        m_T.push_back(p.first);
        m_r.push_back(p.second);
        m_rT_prefix.push_back(p.second * static_cast<double>(p.first));
    }

    const size_t m = m_T.size();
    if (m >= 2) {
        m_r_local.resize(m - 1);
        for (size_t i = 0; i + 1 < m; ++i) {
            unsigned Ti = m_T[i];
            unsigned Ti1 = m_T[i+1];
            double num = m_r[i+1] * static_cast<double>(Ti1) - m_r[i] * static_cast<double>(Ti);
            double den = static_cast<double>(Ti1 - Ti);
            MYASSERT(den > 0.0, "Non-increasing tenor grid detected");
            m_r_local[i] = num / den;
        }
    }
}

double  CurveDiscount::df(const Date& t) const
{
    MYASSERT((!(t < m_today)), "Curve " << m_name << ", DF not available before anchor date " << m_today << ", requested " << t);
    unsigned tau = static_cast<unsigned>(t - m_today);
    unsigned T_last = m_T.back();
    Date last_tenor_date(m_today.serial() + T_last);
    MYASSERT(tau <= T_last, "Curve " << m_name << ", DF not available beyond last tenor date " << last_tenor_date << ", requested " << t);
    
    if (tau == T_last) {
        double r_last = m_r.back();
        return std::exp(- r_last * static_cast<double>(T_last) / 365.0);
    }

    auto it = std::upper_bound(m_T.begin(), m_T.end(), tau);
    size_t idx = static_cast<size_t>(std::distance(m_T.begin(), it));
    MYASSERT(idx > 0 && idx < m_T.size(), "invalid interval lookup");
    size_t i = idx - 1;

    unsigned Ti = m_T[i];
    double rTi = m_rT_prefix[i];
    double r_local = m_r_local[i];
    double dt = static_cast<double>(tau - Ti);
    return std::exp(- (rTi + r_local * dt) / 365.0);
}

} // namespace minirisk
