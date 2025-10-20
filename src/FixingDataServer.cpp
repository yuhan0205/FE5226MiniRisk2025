#include "FixingDataServer.h"
#include "Date.h"
#include "Macros.h"

#include <fstream>
#include <limits>

namespace minirisk {

static unsigned parse_yyyymmdd(const string& s)
{
    MYASSERT(s.size() == 8, "Invalid date format (expected YYYYMMDD): " << s);
    unsigned y = std::stoul(s.substr(0,4));
    unsigned m = std::stoul(s.substr(4,2));
    unsigned d = std::stoul(s.substr(6,2));
    return Date::calendar_to_serial(y,m,d);
}

FixingDataServer::FixingDataServer(const string& filename)
{
    std::ifstream is(filename);
    MYASSERT(!is.fail(), "Could not open file " << filename);
    do {
        string name;
        string yyyymmdd;
        double value;
        is >> name >> yyyymmdd >> value;
        if (!is) break;
        unsigned serial = parse_yyyymmdd(yyyymmdd);
        auto key = std::make_pair(name, serial);
        auto ins = m_fixings.emplace(key, value);
        MYASSERT(ins.second, "Duplicated fixing: " << name << " " << yyyymmdd);
    } while (is);
}

double FixingDataServer::get(const string& name, const Date& t) const
{
    auto it = m_fixings.find(std::make_pair(name, t.serial()));
    MYASSERT(it != m_fixings.end(), "Fixing not found: " << name << " @ " << t.to_string());
    return it->second;
}

std::pair<double, bool> FixingDataServer::lookup(const string& name, const Date& t) const
{
    auto it = m_fixings.find(std::make_pair(name, t.serial()));
    if (it == m_fixings.end())
        return std::make_pair(std::numeric_limits<double>::quiet_NaN(), false);
    return std::make_pair(it->second, true);
}

} // namespace minirisk


