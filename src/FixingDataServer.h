#pragma once

#include <map>
#include <utility>
#include <string>
#include "Global.h"

namespace minirisk {

struct Date;

// Stores historical fixings keyed by (name, date)
struct FixingDataServer
{
public:
    FixingDataServer(const string& filename);

    // return the fixing if available, otherwise trigger an error
    double get(const string& name, const Date& t) const;

    // return the fixing if available, NaN otherwise, and set the flag if found
    std::pair<double, bool> lookup(const string& name, const Date& t) const;

private:
    // key: (name, date-serial)
    std::map<std::pair<string, unsigned>, double> m_fixings;
};

} // namespace minirisk


