#include "basicHl.hpp"

#include "output.hpp"

#include <boost/assert.hpp>
#include <boost/utility/string_ref.hpp>

#include <istream>
#include <limits>

using namespace synth;

namespace {

class CharStream {
public:
    explicit CharStream(std::istream& baseStream)
        : m_stream(baseStream)
        , m_offset(0)
    {}

    CharStream& peek(char& ch)
    {
        if (!m_buf.empty()) {
            ch = m_buf.back();
            return *this;
        }
        int ich = m_stream.peek();
        if (!m_stream.good()) {
            m_stream.setstate(std::ios::failbit);
            return *this;
        }
        assert(ich >= 0);
        assert(ich <= std::numeric_limits<char>::max());
        ch = static_cast<char>(ich);
        return *this;
    }

    CharStream& get(char& ch)
    {
        if (!m_buf.empty()) {
            ch = m_buf.back();
            m_buf.pop_back();
            ++m_offset;
            return *this;
        }
        if (m_stream.get(ch)) {
            ++m_offset;
            assert(m_offset == m_stream.tellg());
        }
        return *this;
    }


    CharStream& skipUntil(char ch)
    {
        for (std::size_t i = 0; i < m_buf.size(); ++i) {
            ++m_offset;
            if (m_buf[i] == ch) {
                m_buf.erase(0, i + 1);
                return *this;
            }
        }
        m_buf.clear();

        if (m_stream) {
            assert(m_offset == m_stream.tellg());
            m_stream.ignore(std::numeric_limits<std::streamsize>::max(), ch);
            std::streampos off = m_stream.tellg();
            assert(off >= 0);
            assert(off <= std::numeric_limits<unsigned>::max());
            m_offset = static_cast<unsigned>(off);
        }
        return *this;
    }

    CharStream& unget(char ch)
    {
        m_buf.push_back(ch);
        --m_offset;
        return *this;
    }
    unsigned tellg() const noexcept { return m_offset; }
    explicit operator bool() const noexcept { return !!m_stream; }

private:
    std::istream& m_stream;
    std::string m_buf;
    unsigned m_offset;
};

static void skipUntilAfter(CharStream& chs, boost::string_ref s)
{
    assert(s.size() > 0);
    while (chs) {
        chs.skipUntil(s[0]);
        for (std::size_t i = 1; i < s.size(); ++i) {
            char gotCh;
            if (!chs.get(gotCh))
                return;
            if (gotCh != s[i]) {
                chs.unget(gotCh);
                for (--i; i > 0; --i)
                    chs.unget(s[i]);
                break;
            }
            if (i + 1 == s.size())
                return;
        }
    }
}

static char skipUntilAny(
    CharStream& chs, char const* s, char const** pFoundCh = nullptr)
{
    char ch;
    while (chs.get(ch)) {
        const char* foundCh = std::strchr(s, ch);
        if (foundCh) {
            if (pFoundCh)
                *pFoundCh = foundCh;
            return ch;
        }
    }
    if (pFoundCh)
        *pFoundCh = nullptr;
    return '\0';
}


struct HlState {
    CharStream in;
    std::vector<Markup>& out;
};

static Markup& createMarkup(
    std::vector<Markup>& markups, unsigned beg, unsigned end) {
    assert(beg < end);
    markups.emplace_back();
    Markup& m = markups.back();
    m.beginOffset = beg;
    m.endOffset = end;
    return m;
}

static Markup& markTillHere(HlState state, unsigned beg) {
    return createMarkup(state.out, beg, state.in.tellg());
}

static bool hlStringNoPrefix(HlState& state, unsigned beg)
{
    char ch;
    if (!state.in.peek(ch))
        return false;
    if (ch == '"') {
        BOOST_VERIFY(state.in.get(ch));
        char const* foundCh;
        for (;;) {
            skipUntilAny(state.in, "\"\\", &foundCh);
            if (!foundCh || *foundCh == '"') {
                markTillHere(state, beg).attrs = TokenAttributes::litStr;
                return true;
            }
        }
    } else if (ch == 'R') {
        BOOST_VERIFY(state.in.get(ch));
        if (!state.in.peek(ch) || ch != '"') {
            state.in.unget('R');
            return false;
        }
        state.in.get(ch);

        std::string delim = ")";
        while (state.in.get(ch) && ch != '(')
            delim.push_back(ch);
        delim += '"';
        skipUntilAfter(state.in, delim);
        markTillHere(state, beg).attrs = TokenAttributes::litStr;
        return true;
    }
    return false;
}

static void hlString(HlState& state)
{
    char ch;
    if (!state.in.peek(ch))
        return;
    if (ch == 'L' || ch == 'U') {
        BOOST_VERIFY(state.in.get(ch));
        if (!hlStringNoPrefix(state, state.in.tellg() - 1))
            state.in.unget(ch);
    } else if (ch == 'u') {
        BOOST_VERIFY(state.in.get(ch));

        if (!state.in.peek(ch))
            return;
        if (ch == '8')
            BOOST_VERIFY(state.in.get(ch));
        else if (ch != '"')
            return;

        if (!hlStringNoPrefix(state, state.in.tellg() - 1 - (ch == '8'))) {
            if (ch == '8')
                state.in.unget('8');
            state.in.unget('u');
        }
    } else {
        hlStringNoPrefix(state, state.in.tellg());
    }
}


static void hlComment(HlState& state)
{
    char ch;
    if (!state.in.peek(ch))
        return;
    unsigned beg = state.in.tellg() - 1;
    if (ch == '/') {
        BOOST_VERIFY(state.in.get(ch));
        if (state.in.skipUntil('\n'))
            state.in.unget('\n');
    } else if (ch == '*') {
        BOOST_VERIFY(state.in.get(ch));
        skipUntilAfter(state.in, "*/");
    } else {
        return;
    }
    markTillHere(state, beg).attrs = TokenAttributes::cmmt;
}

static void hlAdvance(char ch, HlState& state)
{
    static char const kAsciiIdChars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789" "_" "$" /* $ is MS specific */;

    switch (ch) {
        case '/':
            hlComment(state);
            break;
        case '"':
            state.in.unget(ch);
            hlString(state);
            break;
        default:
            // TODO: Here we assume (a) that the encoding is ASCII-compatible and
            // (b) that all non-ascii characters are identifier characters.
            // Ideally we would need get() to return the next Unicode Codepoint and
            // then decide base on
            if (ch >= 0 && ch <= 127
                && (std::strchr(kAsciiIdChars, ch) || ch == '\\')
            ) {
                break;
            }
            hlString(state);
            break;
    }
}

} // anonymous namespace

void synth::basicHighlightFile(std::istream& f, std::vector<Markup>& markups)
{
    char ch;
    HlState state {CharStream(f), markups};
    if (state.in.tellg() == 0)
        hlString(state);
    while (state.in.get(ch))
        hlAdvance(ch, state);
}
