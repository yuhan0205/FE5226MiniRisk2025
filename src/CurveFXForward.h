#pragma once

#include "ICurve.h"
#include "Market.h"

namespace minirisk {

struct CurveFXForward : ICurveFXForward
{
    CurveFXForward(const Market* mkt, const Date& today, const string& name);

    virtual string name() const { return m_name; }
    virtual Date today() const { return m_today; }
    virtual double fwd(const Date& t) const;

private:
    const Market* m_mkt;
    Date m_today;
    string m_name;
    string m_ccy1;
    string m_ccy2;
};

} // namespace minirisk


