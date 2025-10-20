#pragma once
#include "ICurve.h"

namespace minirisk {

struct Market;

struct CurveDiscount : ICurveDiscount
{
    virtual string name() const { return m_name; }

    CurveDiscount(const Market *mkt, const Date& today, const string& curve_name);

    // compute the discount factor
    double df(const Date& t) const;

    virtual Date today() const { return m_today; }

private:
    Date   m_today;
    string m_name;

    std::vector<unsigned> m_T;
    std::vector<double>   m_r;
    std::vector<double>   m_rT_prefix;
    std::vector<double>   m_r_local;
    
};

} // namespace minirisk
