#ifndef SYNTH_SIMPLETEMPLATE_HPP_INCLUDED
#define SYNTH_SIMPLETEMPLATE_HPP_INCLUDED

#include <string>
#include <vector>
#include <iosfwd>
#include <unordered_map>
#include <boost/variant/variant_fwd.hpp>
#include <functional>

namespace synth {

class SimpleTemplate {
public:
    explicit SimpleTemplate(std::string const& text);

    using ValCallback = std::function<void(std::ostream&)>;
    using Val = boost::variant<std::string, ValCallback>;
    using Context = std::unordered_map<std::string, Val>;

    void writeTo(std::ostream& out, Context const& ctx) const;


private:
    std::vector<std::string> m_literals;
    std::vector<std::string> m_insertionKeys;
};

} // namespace synth

#endif // SYNTH_SIMPLETEMPLATE_HPP_INCLUDED
