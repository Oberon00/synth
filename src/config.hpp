#include <boost/config.hpp>

#ifdef BOOST_CLANG
#  define SYNTH_STRINGIFY(x) #x
#  define SYNTH_DISCLANGWARN_BEGIN(w) \
    _Pragma("clang diagnostic push") \
    _Pragma(SYNTH_STRINGIFY(clang diagnostic ignored w))
#  define SYNTH_DISCLANGWARN_END _Pragma("clang diagnostic pop")
#else
#  define SYNTH_DISCLANGWARN_BEGIN(w)
#  define SYNTH_DISCLANGWARN_END
#endif
