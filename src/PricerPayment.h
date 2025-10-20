#pragma once

#include "IPricer.h"
#include "TradePayment.h"

namespace minirisk {

struct PricerPayment : IPricer
{
    PricerPayment(const TradePayment& trd, const std::string& base_ccy);

    virtual double price(Market& m) const;

private:
    double m_amt;
    Date   m_dt;
    string m_ir_curve;
    string m_base_ccy;
    string m_fx_pair; // from trade ccy to base ccy, empty if already base
};

} // namespace minirisk

