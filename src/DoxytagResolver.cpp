#include "DoxytagResolver.hpp"

#include "CgStr.hpp"
#include "output.hpp"
#include "debug.hpp"
#include "xref.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/utility/string_ref.hpp>

#include <iostream>

using namespace synth;

static boost::optional<std::string> getUrl(ptree::ptree const& elem)
{
    auto r = elem.get_optional<std::string>("anchorfile");
    if (!r)
        r = elem.get_optional<std::string>("filename");
    if (!r)
        return r;
    auto anchor = elem.get_child_optional("anchor");
    if (anchor && !anchor->data().empty()) {
        *r += '#';
        r->append(anchor->data());
    }
    return r;
}

DoxytagResolver::DoxytagResolver(ptree::ptree const& tagFileDom, boost::string_ref baseUrl)
    : m_baseUrl(baseUrl)
{
    for (auto const& kv : tagFileDom.get_child("tagfile")) {
        if (kv.first == "compound") {
            parseCompound(kv.second, std::string());
        } else {
            // TODO: Support more tags.
            throw std::runtime_error(
                "Unexpected XML tag in tagfile: " + kv.first);
        }
    }
}

DoxytagResolver DoxytagResolver::fromTagFilename(
    fs::path const& fname, boost::string_ref baseUrl)
{
    ptree::ptree dom;
    fs::ifstream file(fname);
    try {
        file.exceptions(std::ios::failbit | std::ios::badbit);
        ptree::read_xml(file, dom, ptree::xml_parser::no_comments);
    } catch (std::ios::failure const& e) {
        throw std::runtime_error(
            "Error reading tag file " + fname.string() + ": " + e.what());
    }
    return {dom, baseUrl};
}

void DoxytagResolver::link(Markup & m, CXCursor cur)
{
    CXCursor refd = clang_getCursorReferenced(cur);
    if (!isNamespaceLevelDeclaration(refd))
        return;
    auto doxyName = simpleQualifiedName(refd);
    auto it = m_dsts.find(doxyName);
    if (it == m_dsts.end())
        return;
    m.refd = [&dst = it->second, &baseUrl = m_baseUrl] (
        fs::path const&, MultiTuProcessor&)
    {
        return baseUrl + dst;
    };
}

void synth::DoxytagResolver::parseCompound(
    ptree::ptree const& compound, std::string const& prefix)
{
    std::string const* name = addTag(compound, prefix);
    std::string compoundName = name ? prefix + *name + "::" : prefix;
    for (auto const& kv : compound) {
        if (kv.first == "member") {
            addTag(kv.second, compoundName);
        } else if (kv.first == "compound") {
            parseCompound(kv.second, compoundName);
        }
    }
}

std::string const* synth::DoxytagResolver::addTag(
    ptree::ptree const& tag, std::string const& prefix)
{
    auto name = tag.get_optional<std::string>("name");
    if (!name)
        return nullptr;
    auto url = getUrl(tag);
    if (!url)
        return nullptr;
    std::string qname = name->find(':') != std::string::npos
        ? *name : prefix + *name;
    auto insRes = m_dsts.insert({ qname, *std::move(url) });
    if (!insRes.second)
        std::clog << "Duplicate doxytag ignored: " << name << '\n';
    return &insRes.first->first;
}

