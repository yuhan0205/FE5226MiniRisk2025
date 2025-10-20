#include "CurveFXForward.h"
#include "CurveFXSpot.h"
#include "CurveDiscount.h"
#include "Macros.h"
#include "Global.h"

namespace minirisk {

CurveFXForward::CurveFXForward(const Market* mkt, const Date& today, const string& name)
    : m_mkt(mkt)
    , m_today(today)
    , m_name(name)
{
    // Expected naming: "FX.FWD.CCY1.CCY2"
    // Parse to extract CCY1 and CCY2
    size_t first_dot = name.find('.');
    size_t second_dot = (first_dot == string::npos) ? string::npos : name.find('.', first_dot + 1);
    size_t third_dot = (second_dot == string::npos) ? string::npos : name.find('.', second_dot + 1);
    MYASSERT(third_dot != string::npos, "Invalid FX forward curve name format: " << name);
    size_t fourth_dot = name.find('.', third_dot + 1);
    m_ccy1 = name.substr(third_dot + 1, (fourth_dot == string::npos ? name.size() : fourth_dot) - (third_dot + 1));
    if (fourth_dot != string::npos)
        m_ccy2 = name.substr(fourth_dot + 1);
    else {
        size_t last_dot = name.find_last_of('.');
        m_ccy2 = name.substr(last_dot + 1);
        m_ccy1 = name.substr(second_dot + 1, third_dot - second_dot - 1);
    }

    MYASSERT(!m_ccy1.empty() && !m_ccy2.empty(), "Invalid FX forward curve name format: " << name);
}

double CurveFXForward::fwd(const Date& t) const
{
    // F(T0,T) = S(T0) * B1(T0,T) / B2(T0,T)
    // Handle trivial case: same currencies
    if (m_ccy1 == m_ccy2)
        return 1.0;

    // Spot S(T0)
    auto spot_curve = m_mkt->get_fx_spot_curve(fx_spot_name(m_ccy1, m_ccy2));
    double s0 = spot_curve->spot();

    // Discount factors in each currency
    auto disc1 = m_mkt->get_discount_curve(ir_curve_discount_name(m_ccy1));
    auto disc2 = m_mkt->get_discount_curve(ir_curve_discount_name(m_ccy2));

    double b1 = disc1->df(t);
    double b2 = disc2->df(t);

    MYASSERT(b1 > 0.0 && b2 > 0.0, "Invalid discount factors for forward calc: " << b1 << ", " << b2);

    return s0 * (b1 / b2);
}

} // namespace minirisk


