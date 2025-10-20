#include "TradeFXForward.h"
#include "PricerFXForward.h"

namespace minirisk {

const guid_t TradeFXForward::m_id = 3;
const std::string TradeFXForward::m_name = "FX.Forward";

ppricer_t TradeFXForward::pricer(const std::string& configuration) const
{
    return ppricer_t(new PricerFXForward(*this, configuration));
}

} // namespace minirisk
