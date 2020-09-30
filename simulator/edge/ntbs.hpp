//  Copyright (c) 2019 Will Wray https://keybase.io/willwray
//
//  Distributed under the Boost Software License, Version 1.0.
//        http://www.boost.org/LICENSE_1_0.txt
//
//  Repo: https://github.com/willwray/ntbs

#ifndef LTL_NTBS_HPP
#define LTL_NTBS_HPP

#include <cstdint>

/*
   "ntbs.hpp": Concatenation and slicing of 'constexpr C strings'
    ^^^^^^^^
    Here 'NTBS' are constexpr 'null-terminated byte strings' defined as
    char array types of static size, including a char for the expected
    null terminator at size - 1. Embedded null characters may be present.

     namespace ltl::ntbs;

  cat:
     cat(ntbs_args...);      // Concatenate the ntbs_args
     cat<s...>(ntbs_args...) // Join ntbs_args with separator chars "s..."
  cut:
     cut(ntbs_arg)           // Full range [0,end) return == cat(ntbs_arg)
     cut<B>(ntbs_arg);       // Slice range [B,end), a suffix of ntbs_arg
     cut<B,E>(ntbs_arg);     // Slice range [B,E)   (a prefix if B == 0)

  comparison:
     operator==(ntbs, ntbs)  // Compare equality up to and incl. terminal
     operator!=(ntbs, ntbs)  // Compare inequality
     cmp(ntbs, ntbs) -> int  // Lexicographic comparison, as strcmp

   Hello-world example:

     #include "ntbs.hpp"
     using namespace ltl::ntbs;
     constexpr auto hello_world = cat(cat<',',' '>("Hello","world"),'!');
     constexpr auto hello_comma = cut<0,6>(hello_world);  // prefix 6+'\0'
     constexpr auto world_exclaim = cut<-7>(hello_world); // suffix 6+'\0'
                              // == cut<-7,-1>(hello_world)
     #include <cstdio>
     int main() { puts( cat<' '>(hello_comma, world_exclaim) ); }

   Outputs "Hello, world!"

   The return value is of an internal character array class type.
   A const value is usable as a char[N], e.g. as an NTBS argument for
   C-style const char* function parameters (via implicit conversion).
   Non-const values have index access operator[] returning char&.

   Input NTBS args can be char[N] or a generic Char<N> class type.
   Generic access is via free-function 'data' (const only) and 'size':

     data(Char<N>{}) Returns const access to the array begin char & on.
     size(Char<N>{}) Returns array size N, including null terminator.

   Char<N> generalizes string-literal and constexpr char[N] NTBS.

   'Static size' means that size is encoded in the type.
   This lib auto-deduces and tracks sizes - c'est sa raison d'etre.
   The proposed C++20 constexpr std::string will replace many uses
   of these 'sized' ntbs utilites with its 'size-erased' container.

 */

// NTBS_NULL_CHECK Debug switch.
// Follows NDEBUG by default.
// Set to 0 to disable checks, 1 to enable checks
//
#if not defined (NTBS_NULL_CHECK)
#    if not defined (NDEBUG)
#        define NTBS_NULL_CHECK 1
#    else
#        define NTBS_NULL_CHECK 0
#    endif
#endif

namespace ltl {

namespace ntbs {

// copy_n; C++17 constexpr version of std::copy_n, char-specific.
//
constexpr char* copy_n(char const* src, int32_t sz, char* dest)
{
    for (int i = 0; i != sz; ++i)
        dest[i] = src[i];
    return dest + sz;
}

template <int32_t N>
constexpr bool equal(char const* l, char const* r) {
    for (int32_t i = 0; i != N; ++i)
        if ( l[i] != r[i] )
            return false;
    return true;
}

// Generic interface for array type A (for const data access only):
//  * data(A) function returning 'char const*' or 'char const (&)[N]'
//  * size(A) function returning array size N

// char[N] data and size
// Free function data(char[N]) and size(char[N]) overloads for char arrays.
// Override std::size and std::data overloads, from <iterator>, if visible.
//
template <int32_t N>
constexpr auto const& data(char const(&a)[N]) noexcept { return a; }
//
template <int32_t N>
constexpr int32_t size(char const(&)[N]) noexcept { return N; }

// ntbs::array<N>: char array class intended for null-terminated char strings.
// Implicit conversion to char array (ref) and so full decay-to-pointer char*.
//
template <int32_t N>
struct array
{
    char data[N];
    using data_t = char[N];
    constexpr char& operator[](int32_t i) noexcept { return data[i]; }
    constexpr operator data_t const&() const noexcept { return data; }
};

template <int32_t N>
array(char const(&)[N]) -> array<N>;

template <int32_t N>
constexpr auto const* begin(array<N> const& a) noexcept { return a.data; }

template <int32_t N>
constexpr auto const* end(array<N> const& a) noexcept { return a.data + N; }

template <int32_t N>
constexpr auto const& data(array<N> const& a) noexcept { return a.data; }

template <int32_t N>
constexpr int32_t size(array<N> const&) noexcept { return N; }

// ntbs::operator==(L,R) requires L,R are CharArray types Char<Nl>,Char<Nr>.
// No null check; for equal arrays, compares all N characters inlcuding the
// terminating char at index [N -1].
//
template <typename L, typename R>
constexpr bool operator==(L const& lhs, R const& rhs) noexcept
{
    if constexpr( constexpr int32_t N{size(L{})}; N  == size(R{}) )
    {
        for (int32_t i = 0; i != N; ++i)
            if ( data(lhs)[i] != data(rhs)[i] )
                return false;
        return true;
    }
    else
        return false;
}

// ntbs::operator!=(L,R) requires L,R are CharArray types.
//
template <typename L, typename R>
constexpr bool operator!=(L const& lhs, R const& rhs) noexcept {
    return !(lhs == rhs);
}

// ntbs::cmp(L,R) requires L,R are CharArray types.
// Lexicographic compare, constexpr version of strcmp
// (UB in non-constexpr call with non-null-terminated data and no null check).
//
template <typename L, typename R>
constexpr int32_t cmp(L const& lhs, R const& rhs) noexcept(!NTBS_NULL_CHECK)
{
#if NTBS_NULL_CHECK
    if ( data(lhs)[size(L{}) - 1] != 0
      || data(rhs)[size(R{}) - 1] != 0 )
        throw "ntbs::cmp arg not null-terminated";
#endif
    int32_t i = 0;
    while ( data(lhs)[i] != 0 && data(lhs)[i] == data(rhs)[i] )
        ++i;
    return static_cast<unsigned char>(data(lhs)[i])
         - static_cast<unsigned char>(data(rhs)[i]);
}

// cut<B,E>(Char<N>) Returns sub-array [B,E) of char array arg.
// The result is returned as ntbs::array<M> type with zero terminator.
// The [B,E) indices are signed integers (c.f. Python slice indexing).
// The input char array argument is assumed to be zero terminated.
//
template <int32_t B = 0, int32_t E = -1, typename A>
constexpr
auto
cut(A const& a) noexcept(!NTBS_NULL_CHECK)
{
    constexpr int32_t N = size(A{});
#if NTBS_NULL_CHECK
    if ( data(a)[N - 1] != 0 )
        throw "ntbs::cut arg not null-terminated";
#endif
    constexpr auto ind = [](int32_t i, int32_t N) -> int32_t {
        return i < 0 ? N + i : i;
    };
    constexpr int32_t b = ind(B,N), e = ind(E,N);
    static_assert( 0 <= b && b <= e && e <= N,
                   "index out of bounds");
    constexpr int32_t M = e - b;
    array<M + 1> chars{};
    copy_n( data(a) + b, M, chars.data);
    return chars;
}

// Single-char data and size.
// Free function data(c) and size(c) overloads for char c.
// Note that size is 2, as-if a null-terminated char string.
// Declared here to allow char args in cat.
//
constexpr char const* data(char const& c) noexcept { return &c; }
//
constexpr int32_t size(char const&) noexcept { return 2; }

// cat(cs...) Concatenate input character sequences, char or ntbs, or
// join the inputs using variadic char template args as separator.
// Return in an ntbs::array<M> type with zero terminator.
//
template <char... sep,
      typename... Cs>
constexpr
auto
cat(Cs const&... cs) noexcept(!NTBS_NULL_CHECK)
{
#if NTBS_NULL_CHECK
    ([](char const* dat) {
        if constexpr ( sizeof(Cs) != 1 || size(Cs{}) == 1 )
            if ( dat[sizeof(Cs) - 1] != 0 )
                throw "ntbs::cat arg not null-terminated";
    }(data(cs)), ...);
#endif
    constexpr int32_t seps{ sizeof...(sep) };
    array<((size(Cs{}) -1 + seps) + ...
                       + (1 - seps))> acc{};
    char* p = nullptr;
    ((p = p == nullptr ? acc.data : (((*p++ = sep), ...), p),
        p = copy_n(data(cs), size(Cs{})-1, p)), ...);
    return acc;
}

} // namespace ntbs

} // namespace ltl

#endif
