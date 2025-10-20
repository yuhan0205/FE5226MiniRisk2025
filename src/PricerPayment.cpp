#include "PricerPayment.h"
#include "TradePayment.h"
#include "CurveDiscount.h"
#include "CurveFXSpot.h"

namespace minirisk {

PricerPayment::PricerPayment(const TradePayment& trd, const std::string& base_ccy)
    : m_amt(trd.quantity())
    , m_dt(trd.delivery_date())
    , m_ir_curve(ir_curve_discount_name(trd.ccy()))
    , m_base_ccy(base_ccy)
    , m_fx_pair(trd.ccy() == base_ccy ? "" : fx_spot_name(trd.ccy(), base_ccy))
{
}

double PricerPayment::price(Market& mkt) const
{
    ptr_disc_curve_t disc = mkt.get_discount_curve(m_ir_curve);
    double df = disc->df(m_dt); // this throws an exception if m_dt<today

    // This PV is expressed in trade ccy. Convert into base currency if needed.
    if (!m_fx_pair.empty()) {
        ptr_fx_spot_curve_t fx_spot = mkt.get_fx_spot_curve(m_fx_pair);
        df *= fx_spot->spot();
    }

    return m_amt * df;
}

} // namespace minirisk


