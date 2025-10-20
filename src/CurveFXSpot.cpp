#include "CurveFXSpot.h"
#include "Macros.h"
#include "Global.h"
#include <algorithm>
#include <cmath>

namespace minirisk {

CurveFXSpot::CurveFXSpot(const Market* mkt, const Date& today, const string& name)
    : m_mkt(mkt)
    , m_today(today)
    , m_name(name)
{
    // Parse the name to extract currency pair
    // Expected format: "FX.SPOT.CCY1.CCY2" or "FX.SPOT.CCY1" (for CCY1USD)
    size_t pos = name.find_last_of('.');
    if (pos != string::npos) {
        string suffix = name.substr(pos + 1);
        if (suffix == "USD") {
            // Format: FX.SPOT.CCY1.USD -> CCY1USD
            size_t prev_pos = name.find_last_of('.', pos - 1);
            if (prev_pos != string::npos) {
                m_ccy1 = name.substr(prev_pos + 1, pos - prev_pos - 1);
                m_ccy2 = "USD";
            }
        } else {
            // Format: FX.SPOT.CCY1 -> CCY1USD
            m_ccy1 = suffix;
            m_ccy2 = "USD";
        }
    }
    
    MYASSERT(!m_ccy1.empty() && !m_ccy2.empty(), 
        "Invalid FX spot curve name format: " << name);
}

double CurveFXSpot::spot() const
{
    // Handle different currency pair types
    if (m_ccy1 == "USD" && m_ccy2 == "USD") {
        return 1.0; // USD/USD = 1.0
    }
    
    if (m_ccy2 == "USD") {
        // Direct pair: CCY1USD
        return m_mkt->get_fx_spot(fx_spot_name(m_ccy1, "USD"));
    }
    
    if (m_ccy1 == "USD") {
        // Inverse pair: USDCCY2
        double ccy2_usd = m_mkt->get_fx_spot(fx_spot_name(m_ccy2, "USD"));
        MYASSERT(ccy2_usd > 0, "Invalid FX rate for " << m_ccy2 << ": " << ccy2_usd);
        return 1.0 / ccy2_usd;
    }
    
    // Cross currency pair: CCY1CCY2
    // We need to convert CCY1 -> USD -> CCY2
    double ccy1_usd = m_mkt->get_fx_spot(fx_spot_name(m_ccy1, "USD"));
    double ccy2_usd = m_mkt->get_fx_spot(fx_spot_name(m_ccy2, "USD"));
    
    MYASSERT(ccy1_usd > 0, "Invalid FX rate for " << m_ccy1 << ": " << ccy1_usd);
    MYASSERT(ccy2_usd > 0, "Invalid FX rate for " << m_ccy2 << ": " << ccy2_usd);
    
    return ccy1_usd / ccy2_usd;
}

} // namespace minirisk
