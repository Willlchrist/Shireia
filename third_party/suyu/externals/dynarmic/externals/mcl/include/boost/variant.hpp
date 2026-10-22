// Forwarding header: redirect to whichever boost is actually present.
//
// Upstream committed this file with an absolute path into the original
// developer's home directory, so it built on exactly one machine.
//
// This is a shim sitting at boost/variant.hpp inside mcl's own include
// directory, shadowing the real umbrella header of the same name. Two things
// follow from that, and both have already been got wrong here:
//
//   - It cannot include <boost/variant.hpp>, because that is itself.
//   - It cannot just include <boost/variant/variant.hpp> either. The umbrella
//     pulls in recursive_wrapper, apply_visitor, get and several more, and
//     dynarmic's IR::Term uses boost::recursive_wrapper - so forwarding to the
//     core header alone leaves it an incomplete type.
//
// #include_next is built for precisely this: it resumes the search *after* the
// directory this file was found in, so it reaches the real umbrella with all
// of its contents. GCC and Clang support it; MSVC does not, but MSVC does not
// need it - on Windows there is no boost on the include path at all and the
// CPM fallback below is the only route.
#if defined(__GNUC__) || defined(__clang__)
#  if defined(__has_include_next)
#    if __has_include_next(<boost/variant.hpp>)
#      include_next <boost/variant.hpp>
#      define MCL_BOOST_VARIANT_RESOLVED
#    endif
#  endif
#endif

#ifndef MCL_BOOST_VARIANT_RESOLVED
// The Windows case: CPM downloads boost into the consuming project's cache and
// does not put it on the include path, so name it directly. The version is
// pinned here because there is no way to glob from the preprocessor; generating
// this header with configure_file from the CPM variables would fix that, but it
// needs the consuming project's boost target rather than anything dynarmic
// knows on its own.
#  include "../../../../../../.cache/cpm/boost/boost-1.90.0/libs/variant/include/boost/variant.hpp"
#endif
