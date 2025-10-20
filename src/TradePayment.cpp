#include "TradePayment.h"
#include "PricerPayment.h"

namespace minirisk {

ppricer_t TradePayment::pricer(const std::string& configuration) const
{
    return ppricer_t(new PricerPayment(*this, configuration));
}

} // namespace minirisk
