#include "simpletemplate.hpp"
#include <ostream>
#include <cassert>
#include <boost/variant/variant.hpp>
#include <boost/variant/apply_visitor.hpp>

using namespace synth;

SimpleTemplate::SimpleTemplate(std::string const& text)
{
    static char const marker[] = "@@";
    static unsigned const markerLen = sizeof(marker);

    std::string::size_type last = 0;
    auto beg = text.find(marker);
    while (beg != std::string::npos) {
        auto end = text.find(marker, beg + markerLen);
        if (end == std::string::npos) {
            m_literals.push_back(text.substr(beg));
            break;
        }
        m_literals.push_back(text.substr(last, beg - last));
        m_insertionKeys.push_back(text.substr(
            beg + markerLen - 1, end - beg - markerLen + 1));
        last = end + markerLen - 1;
        beg = text.find(marker, last);
    }
    m_literals.push_back(text.substr(last));
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
