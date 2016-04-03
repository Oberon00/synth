#include "SimpleTemplate.hpp"

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/variant.hpp>

#include <cassert>
#include <ostream>

using namespace synth;

SimpleTemplate::SimpleTemplate(boost::string_ref text)
{
    static char const rawMarker[] = "@@";
    static boost::string_ref constexpr marker(rawMarker, sizeof(rawMarker) - 1);

    auto beg = text.find(marker);
    while (beg != boost::string_ref::npos) {
        m_literals.emplace_back(text.data(), beg);
        text.remove_prefix(beg + marker.size());
        auto end = text.find(marker);
        if (end == boost::string_ref::npos) {
            std::string& lit = m_literals.back();
            lit.reserve(lit.size() + marker.size() + text.size());
            lit.append(marker.data(), marker.size());
            lit.append(text.data(), text.size());
            assert(m_literals.size() == m_insertionKeys.size() + 1);
            return;
        }
        m_insertionKeys.emplace_back(text.data(), end);
        text.remove_prefix(end + marker.size());
        beg = text.find(marker);
    }
    m_literals.emplace_back(text.data(), text.size());
    assert(m_literals.size() == m_insertionKeys.size() + 1);
}

namespace {

class ValVisitor: public boost::static_visitor<> {
public:
    ValVisitor(std::ostream& out)
        : m_out(out)
    { }

    void operator()(std::string const& s) { m_out << s; }
    void operator()(SimpleTemplate::ValCallback const& cb) { cb(m_out); }

private:
    std::ostream& m_out;
};

} // anonymous namespace

void SimpleTemplate::writeTo(
    std::ostream& out,
    SimpleTemplate::Context const& ctx) const
{
    ValVisitor visitor(out);
    for (std::size_t i = 0; i < m_literals.size(); ++i) {
        out << m_literals[i];
        if (m_insertionKeys.size() > i) {
            auto it = ctx.find(m_insertionKeys[i]);
            if (it == ctx.end()) {
                throw std::runtime_error(
                    "No value for template placeholder @@"
                    + m_insertionKeys[i]
                    + "@@ available.");
            }
            boost::apply_visitor(visitor, it->second);
        }
    }
}
