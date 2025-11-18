#include "PricerFXForward.h"
#include "TradeFXForward.h"
#include "CurveDiscount.h"
#include "CurveFXForward.h"
#include "CurveFXSpot.h"
#include "Global.h"

namespace minirisk {

PricerFXForward::PricerFXForward(const TradeFXForward& trd, const std::string& base_ccy)
    : m_notional(trd.quantity())
    , m_ccy1(trd.ccy1())
    , m_ccy2(trd.ccy2())
    , m_strike(trd.strike())
    , m_fixing_date(trd.fixing_date())
    , m_settle_date(trd.settle_date())
    , m_base_ccy(base_ccy)
    , m_fx_pair(m_ccy2 == base_ccy ? "" : fx_spot_name(m_ccy2, base_ccy))
{
}

double PricerFXForward::price(Market& mkt, const FixingDataServer* fds) const
{
    Date T0 = mkt.today(); // pricing date
    Date T1 = m_fixing_date; // fixing date
    Date T2 = m_settle_date; // settlement date
    
    // Check for expired trade: settlement date must be on or after pricing date
    MYASSERT(!(T0 > T2), "Trade is expired: settlement date " << T2.to_string() 
            << " is before pricing date " << T0.to_string());
    
    // Get discount curve for ccy2 (settlement currency)
    ptr_disc_curve_t disc_ccy2 = mkt.get_discount_curve(ir_curve_discount_name(m_ccy2));
    double b2 = disc_ccy2->df(m_settle_date); // B2(T0, T2)
    
    double spot_price;
    
    // Determine which spot price to use based on the relationship between T0, T1, T2
    if (T0 < T1) {
        // Scenario 1: T0 < T1 - both fixing and settlement are in the future
        // Use forward price F(T0, T1)
        ptr_fx_fwd_curve_t fx_fwd = mkt.get_fx_fwd_curve(fx_fwd_name(m_ccy1, m_ccy2));
        spot_price = fx_fwd->fwd(m_fixing_date);
    }
    else if (T0 == T1) {
        // Scenario 2: T0 = T1 - check if fixing is available
        if (fds) {
            // Try to get historical fixing first
            auto fixing_result = fds->lookup(fx_spot_name(m_ccy1, m_ccy2), T1);
            if (fixing_result.second) {
                // Historical fixing is available, use it
                spot_price = fixing_result.first;
            } else {
                // Fixing not available, use forward price (full delta and PV01 risk)
                ptr_fx_fwd_curve_t fx_fwd = mkt.get_fx_fwd_curve(fx_fwd_name(m_ccy1, m_ccy2));
                spot_price = fx_fwd->fwd(m_fixing_date);
            }
        } else {
            // No fixing data server, use forward price
            ptr_fx_fwd_curve_t fx_fwd = mkt.get_fx_fwd_curve(fx_fwd_name(m_ccy1, m_ccy2));
            spot_price = fx_fwd->fwd(m_fixing_date);
        }
    }
    else if (T1 < T0 && T0 < T2) {
        // Scenario 3: T1 < T0 < T2 - fixing date has passed, settlement in future
        // Use historical fixing (no delta risk with underlying, but PV01 risk remains)
        if (fds) {
            spot_price = fds->get(fx_spot_name(m_ccy1, m_ccy2), T1);
        } else {
            MYASSERT(false, "Historical fixing required for date " << T1.to_string() 
                    << " but no fixing data server provided");
        }
    }
    else if (T0 == T2) {
        // Scenario 4: T0 = T2 - settlement date is today
        // Use historical fixing (no delta with underlying or PV01 risk)
        if (fds) {
            spot_price = fds->get(fx_spot_name(m_ccy1, m_ccy2), T1);
        } else {
            MYASSERT(false, "Historical fixing required for date " << T1.to_string() 
                    << " but no fixing data server provided");
        }
    }
    else {
        // T0 > T2 case is already handled by the expired trade check above
        // This else clause should only catch other unexpected date relationships
        MYASSERT(false, "Unexpected date relationship: T0=" << T0.to_string() 
                << ", T1=" << T1.to_string() << ", T2=" << T2.to_string());
    }
    
    // Calculate price in ccy2: price = B2(T0, T2)[S(T1) - K]
    // Note: S(T1) is either the forward price F(T0, T1) or historical fixing
    double price_ccy2 = b2 * (spot_price - m_strike);
    
    // Convert to base currency if needed
    if (!m_fx_pair.empty()) {
        ptr_fx_spot_curve_t fx_spot = mkt.get_fx_spot_curve(m_fx_pair);
        price_ccy2 *= fx_spot->spot();
    }
    
    return m_notional * price_ccy2;
}

} // namespace minirisk
