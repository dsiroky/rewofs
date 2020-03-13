/// Place this include above thirdparty includes. Pair with enablewarnings.hpp.
///
/// Project has very restrictive warnings and lots of libraries does not
/// conform to that. MSVC++ does not have "-isystem" equivalent.
///
/// @file

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
#pragma GCC diagnostic ignored "-Wdeprecated"
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wsign-promo"
#pragma GCC diagnostic ignored "-Wswitch-enum"
#pragma GCC diagnostic ignored "-Wswitch-default"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#ifndef __APPLE__
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif
#ifdef __clang__
#ifndef __APPLE__
  #pragma GCC diagnostic ignored "-Winconsistent-missing-override"
#endif
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma GCC diagnostic ignored "-Wparentheses-equality"
#if __clang_major__ * 100 + __clang_minor__ >= 309
  #ifndef __APPLE__
    #pragma GCC diagnostic ignored "-Wexpansion-to-defined"
  #endif
  #pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
#endif
