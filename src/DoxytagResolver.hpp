#ifndef SYNTH_DOXYTAG_RESOLVER_HPP_INCLUDED
#define SYNTH_DOXYTAG_RESOLVER_HPP_INCLUDED

#include <clang-c/Index.h>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/utility/string_ref_fwd.hpp>

#include <unordered_map>

namespace synth {

struct Markup;

namespace ptree = boost::property_tree;
namespace fs = boost::filesystem;

class DoxytagResolver {
public:
    DoxytagResolver(ptree::ptree const& tagFileDom, boost::string_ref baseUrl);

    static DoxytagResolver fromTagFilename(
        fs::path const& fname, boost::string_ref baseUrl);

    void link(Markup& m, CXCursor cur);

private:
    void parseCompound(ptree::ptree const& compound, std::string const& prefix);
    std::string const* addTag(ptree::ptree const& tag, std::string const& prefix);

    std::string m_baseUrl;
    std::unordered_map<std::string, std::string> m_dsts;
};

} // namespace synth

#endif // SYNTH_DOXYTAGRESOLVER_HPP_INCLUDED
