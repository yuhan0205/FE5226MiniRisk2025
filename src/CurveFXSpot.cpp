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
    // Supported formats:
    //  - "FX.SPOT.CCY1.CCY2" (general pair)
    //  - "FX.SPOT.CCY1" (interpreted as CCY1/USD)
    size_t first_dot = name.find('.');
    size_t second_dot = (first_dot == string::npos) ? string::npos : name.find('.', first_dot + 1);
    size_t third_dot = (second_dot == string::npos) ? string::npos : name.find('.', second_dot + 1);
    if (third_dot != string::npos) {
        // General pair FX.SPOT.CCY1.CCY2
        size_t fourth_dot = name.find('.', third_dot + 1);
        // Expect no fourth dot; tokens: FX, SPOT, CCY1, CCY2
        m_ccy1 = name.substr(third_dot + 1, (fourth_dot == string::npos ? name.size() : fourth_dot) - (third_dot + 1));
        // Extract CCY2 after the next dot if present, otherwise already captured
        if (fourth_dot != string::npos) {
            m_ccy2 = name.substr(fourth_dot + 1);
        } else {
            // No extra dot, m_ccy1 actually holds CCY1, need to find CCY2 after the third dot
            size_t last_dot = name.find_last_of('.');
            m_ccy2 = name.substr(last_dot + 1);
            // Fix m_ccy1 to be token between second and third dots
            m_ccy1 = name.substr(second_dot + 1, third_dot - second_dot - 1);
        }
    } else if (second_dot != string::npos) {
        // Short form FX.SPOT.CCY1 (interpreted as CCY1/USD)
        m_ccy1 = name.substr(second_dot + 1);
        m_ccy2 = "USD";
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
    
    // Cross currency pair: CCY1/CCY2
    // We need to convert CCY1 -> USD -> CCY2
    double ccy1_usd = m_mkt->get_fx_spot(fx_spot_name(m_ccy1, "USD"));
    double ccy2_usd = m_mkt->get_fx_spot(fx_spot_name(m_ccy2, "USD"));
    
    MYASSERT(ccy1_usd > 0, "Invalid FX rate for " << m_ccy1 << ": " << ccy1_usd);
    MYASSERT(ccy2_usd > 0, "Invalid FX rate for " << m_ccy2 << ": " << ccy2_usd);
    
    return ccy1_usd / ccy2_usd;
}

} // namespace minirisk
