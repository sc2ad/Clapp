#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

#include <cassert>

#include "macro_sequence_for.h"

// SFINAE for determining how many members we have
// THEN, we try each, attempting to perform an argument parse from each

// The argument parser specialization
template <class T> struct ArgParse;

// If we failed to convert into a type
struct ParseError {};

template <class T> using ArgParseReturnT = std::variant<T, ParseError>;

template <class T>
concept is_parsable =
    requires(const char **&begin, const char *const *const end) {
      {
        ArgParse<T>::Parse(begin, end)
      } -> std::convertible_to<ArgParseReturnT<T>>;
    };

// If we called with --help or are missing positional arguments
struct UsageError {};

// Represents an unknown argument or flag that cannot be parsed
struct UnknownArgError {};

// Need to handle at compile time:
/*

In the format string, a suitable format specifier will be used for builtin types
that Clang knows how to format. This includes standard builtin types, as well as
aggregate structures, void* (printed with %p), and const char* (printed with
%s). A *%p specifier will be used for a field that Clang doesn’t know how to
format, and the corresponding argument will be a pointer to the field. This
allows a C++ templated formatting function to detect this case and implement
custom formatting. A * will otherwise not precede a format specifier.

AND
%c	Character
%d	Signed integer
%e or %E	Scientific notation of floats
%f	Float values
%g or %G	Similar as %e or %E
%hi	Signed integer (short)
%hu	Unsigned Integer (short)
%i	Unsigned integer
%l or %ld or %li	Long
%lf	Double
%Lf	Long double
%lu	Unsigned int or unsigned long
%lli or %lld	Long long
%llu	Unsigned long long
%o	Octal representation
%p	Pointer
%s	String
%u	Unsigned int
*/

// Two ideas:
// 1. constexprify this and use template params to describe all the metainfo
// This should be doable because we will know on compile time how we want to
// perform these transformations
// 2. just have a normal type that we fill out and use, that just has the
// members listed that can be set
struct Options {
  // The name to override the member name with
  std::string name;
  // To disallow multiflag support
  bool disallow_multiflag;
  // TODO: To state that a flag is required
  bool required;
  // If an argument is positional and not a flag
  bool positional;
};

// The metainfo for a given type to be parsed. Has sane defaults but can be
// specialized.
template <class T> struct MetaInfo {
  constexpr static std::string_view OptionsPrefix = "__";
  constexpr static std::array help_args{"--help", "--h"};
  constexpr static bool extra_args_ok = true;
};

namespace detail {

template <class T, class U> struct Combine {
  using first = T;
  using second = U;
};

template <class... TArgs> struct RetTrait {
  using type = std::array<const char *, sizeof...(TArgs)>;
};

template <class T> struct MemberRef {
  MemberRef(T &t_ref, const char *name) : field_ref(t_ref), name(name) {}

  T &field_ref;
  std::string_view name;
};

template <size_t I, size_t... Is>
auto make_member_refs(auto const &member_names, auto &...bindings) {
  if constexpr (sizeof...(Is) != 0) {
    return std::tuple_cat(
        std::make_tuple(MemberRef(std::get<I>(std::tie(bindings...)),
                                  std::get<I>(member_names))),
        make_member_refs<Is...>(member_names, bindings...));
  } else {
    return std::make_tuple(MemberRef(std::get<I>(std::tie(bindings...)),
                                     std::get<I>(member_names)));
  }
}

template <size_t... Is>
auto make_member_refs_wrap(auto const &member_names, std::index_sequence<Is...>,
                           auto &...bindings) {
  return make_member_refs<Is...>(member_names, bindings...);
}

template <class T, class F>
decltype(auto) dump_type(T &&val, F &&f, auto &...bindings) {
  typename RetTrait<decltype(bindings)...>::type member_names{};
  auto itr = member_names.begin();
  // TODO: Do this logic at compile time
  auto lamb = [&itr](auto &&, auto &&...args) {
    if constexpr (sizeof...(args) >= 3) {
      auto tup = std::make_tuple(args...);
      if (std::get<0>(tup) == "  ") {
        // Then this is a TOP LEVEL member!
        // Name is: std::get<1>(tup)
        *itr++ = std::get<2>(tup);
      }
    }
  };
  __builtin_dump_struct(&val, lamb);
  auto tuple = make_member_refs_wrap(
      member_names, std::make_index_sequence<sizeof...(bindings)>{},
      bindings...);
  // Now combine our member names with our binding references into MemberRef
  // instances
  std::forward<F>(f)(std::move(tuple));
}

template <class... Ts> struct OverloadSet : Ts... {
  constexpr OverloadSet(Ts &&...ts) : Ts(std::move(ts))... {}
  using Ts::operator()...;
  constexpr static auto size = sizeof...(Ts);
};

template <class... Ts> constexpr auto MakeOverloadedSet(Ts &&...ts) {
  return OverloadSet<Ts...>(std::forward<Ts>(ts)...);
}

constexpr int CountOccurances(const char *str, char c) {
  return *str == '\0' ? 0 : (*str == c) + CountOccurances(str + 1, c);
}

// We count our arguments by counting how many ) there are
// since we will always use (_1)(_2) ... (_N) for our values
#define NUMARGS(...) (detail::CountOccurances(#__VA_ARGS__, ')'))

// IMPORTED HEADER

// Decltype-ify and comma-ify our large macros for expansion
#define DECLTYPE_BODY(n, d, ...) d(decltype(__VA_ARGS__))
#define DECLTYPE_ADD_REF_BODY(n, d, ...)                                       \
  d(std::add_lvalue_reference_t<decltype(__VA_ARGS__)>)
#define DECLVAL_BODY(n, d, ...) d(std::declval<decltype(__VA_ARGS__)>())
#define IDENTITY_BODY(n, d, ...) d(__VA_ARGS__)

#define IDENTITY(...) __VA_ARGS__
#define STEP_CALLER(...) , __VA_ARGS__

#define COMMA ,
#define COMMA_STEP(n, d, ...) STEP_CALLER

#define DECLTYPEIFY(...)                                                       \
  SF_FOR_EACH(DECLTYPE_BODY, COMMA_STEP, SF_NULL, IDENTITY, __VA_ARGS__)
#define DECLTYPEIFY_ADD_REF(...)                                               \
  SF_FOR_EACH(DECLTYPE_ADD_REF_BODY, COMMA_STEP, SF_NULL, IDENTITY, __VA_ARGS__)
#define DECLVALIFY(...)                                                        \
  SF_FOR_EACH(DECLVAL_BODY, COMMA_STEP, SF_NULL, IDENTITY, __VA_ARGS__)
#define NAMES(...)                                                             \
  SF_FOR_EACH(IDENTITY_BODY, COMMA_STEP, SF_NULL, IDENTITY, __VA_ARGS__)

#define UNPACK_IMPL(...)                                                       \
  [](auto &&t, auto &&f) -> decltype(({                                        \
    auto &&[NAMES(__VA_ARGS__)] = std::forward<decltype(t)>(t);                \
    (void)(std::tie(NAMES(__VA_ARGS__)));                                      \
    std::declval<void>();                                                      \
  })) {                                                                        \
    auto &&[NAMES(__VA_ARGS__)] = std::forward<decltype(t)>(t);                \
    dump_type(std::forward<decltype(t)>(t), std::forward<decltype(f)>(f),      \
              NAMES(__VA_ARGS__));                                             \
  }

template <class T, class V, class F>
concept is_unpackable = requires(T &&t, V &&v, F &&f) {
  { t(std::forward<V>(v), std::forward<F>(f)) };
};

// Helper to report errors for unpacking
template <class T, bool err, auto sz> struct UnpackableWrapperError {
  constexpr static bool value = err;
  constexpr static auto size = sz;
};

template <class T, class F> auto const &do_on_bindings_impl(T &&t, F &&f) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
  // clang-format off
  constexpr static auto set = MakeOverloadedSet(
UNPACK_IMPL((_0)),
UNPACK_IMPL((_0)(_1)),
UNPACK_IMPL((_0)(_1)(_2)),
UNPACK_IMPL((_0)(_1)(_2)(_3)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)(_49)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)(_49)(_50)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)(_49)(_50)(_51)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)(_49)(_50)(_51)(_52)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)(_49)(_50)(_51)(_52)(_53)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)(_49)(_50)(_51)(_52)(_53)(_54)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)(_49)(_50)(_51)(_52)(_53)(_54)(_55)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)(_49)(_50)(_51)(_52)(_53)(_54)(_55)(_56)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)(_49)(_50)(_51)(_52)(_53)(_54)(_55)(_56)(_57)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)(_49)(_50)(_51)(_52)(_53)(_54)(_55)(_56)(_57)(_58)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)(_49)(_50)(_51)(_52)(_53)(_54)(_55)(_56)(_57)(_58)(_59)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)(_49)(_50)(_51)(_52)(_53)(_54)(_55)(_56)(_57)(_58)(_59)(_60)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)(_49)(_50)(_51)(_52)(_53)(_54)(_55)(_56)(_57)(_58)(_59)(_60)(_61)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)(_49)(_50)(_51)(_52)(_53)(_54)(_55)(_56)(_57)(_58)(_59)(_60)(_61)(_62)),
UNPACK_IMPL((_0)(_1)(_2)(_3)(_4)(_5)(_6)(_7)(_8)(_9)(_10)(_11)(_12)(_13)(_14)(_15)(_16)(_17)(_18)(_19)(_20)(_21)(_22)(_23)(_24)(_25)(_26)(_27)(_28)(_29)(_30)(_31)(_32)(_33)(_34)(_35)(_36)(_37)(_38)(_39)(_40)(_41)(_42)(_43)(_44)(_45)(_46)(_47)(_48)(_49)(_50)(_51)(_52)(_53)(_54)(_55)(_56)(_57)(_58)(_59)(_60)(_61)(_62)(_63))
  );
  // clang-format on
#pragma GCC diagnostic pop
  constexpr static bool unpackable = is_unpackable<decltype(set), T, F>;
  // We return solely for type deduction purposes so that we get better error
  // reporting
  if constexpr (unpackable) {
    set(std::forward<T>(t), std::forward<F>(f));
  }
  return set;
};

#undef NUMARGS
#undef UNPACK_IMPL

template <class T, class F> constexpr auto const &do_on_bindings(T &t, F &&f) {
  return do_on_bindings_impl(t, std::forward<F>(f));
}

// Because things like std::vector exist
// and because we want to support multiflags
// We need to actually walk all of the argv FIRST
// then for each argv we see that matches (either -- or not for our params)
// we call the custom parse with a reference
// For vectors, that's great, we just push back
// For other types, we just overwrite

// Special case for --help: don't let that be overriden
// If we see any argv that is --help, we stop and print our help message

// Return a map of names to Options
template <class T, class U> decltype(auto) get_arg_options(U &&members_tuple) {
  // TODO: Could be constexpr array?
  // TODO: If we swap Options to use types, this will need to change to be a
  // tuple
  std::unordered_map<std::string_view, Options> map;
  using meta_type = MetaInfo<T>;
  std::apply(
      [&map](auto &...members) {
        // For each member:
        (
            [&map](auto &memb) {
              if constexpr (std::is_convertible_v<decltype(memb.field_ref),
                                                  Options>) {
                // If the name does not start with meta_type::OptionsPrefix, we
                // abort safely
                if (memb.name.starts_with(meta_type::OptionsPrefix)) {
                  map.emplace(
                      std::string_view(
                          &memb.name.data()[meta_type::OptionsPrefix.size()]),
                      memb.field_ref);
                } else {
                  // TODO: Fail cleanly with good error message
                  assert(false);
                }
              }
            }(members),
            ...);
        // If the type matches, check the name
        // If the name is prefixed with OPTIONS_PREFIX, add it to the map of
        // names to be returned
      },
      members_tuple);
  return map;
}

// See if any of the args listed match the help args, return true if so
template <class T> constexpr bool is_help(const char *arg) {
  using meta_type = MetaInfo<T>;
  return std::any_of(meta_type::help_args.begin(), meta_type::help_args.end(),
                     [arg](const char *a) { return arg == a; });
}

// We create our help message by walking our members and doing a few things:
// 1. write our converted name (ex: a for positional --a for optional) (TODO:
// Handle aliases?)
// 2. write our type (ex: int, list, str, etc., grabbed from template
// specialization)
// 3. write our default (done by reading the current value when we do the parse,
// also done by specialization)

// The usage line is given by argv[0] and all of our positional arguments

template <class T, class F>
void on_positionals(T &member, auto const &options_map, F &&func) {
  auto itr = options_map.find(member.name);
  auto const options = itr != options_map.end() ? itr->second : Options{};
  if (options.positional) {
    func(member, options);
  }
}

template <class T> void display_help(T &inst, const char *program_name) {
  // TODO: Display help message from members
  // printf("HELP MESSAGE: %p: %s\n", static_cast<void *>(&inst), program_name);
  // Get our options map: member name --> Option if it exists
  std::unordered_map<std::string_view, Options> options_map;
  do_on_bindings(inst, [&options_map](auto &&members) {
    options_map = get_arg_options<T>(members);
  });
  printf("Usage: %s", program_name);
  // Walk the members again, if they are positional, we write them out
  // TODO: Or required flags
  do_on_bindings(
      inst, [&options_map = std::as_const(options_map)](auto &&members) {
        std::apply(
            [&options_map](auto &...members) {
              (on_positionals(members, options_map,
                              [](auto &member, auto const &options) {
                                std::string name = options.name.empty()
                                                       ? member.name.data()
                                                       : options.name;
                                printf(" <%s>", name.c_str());
                              }),
               ...);
              // After all of the positionals are written, we write a newline
              puts("");
            },
            members);
      });
  // Then, write our flags and the types of them, as well as any default values
}

// Example:
/*

struct Test {
    int x;
    bool should_exist;
    Options __x{.positional = true};
};

*/

// Is it easier to walk through the argv and then parse to a member?
// That helps with positionals
// ex: if (!arg.starts_with("-")) {(parse_positional(memb, arg,
// positional_counter), ...)} parse_positional: if(memb is positional)
// parse_to<T>(arg); positional_counter++;

// How to handle multiflags in multiple locations with spaces? Easy, parse one
// and done, then parse next

// to parse a positional, we track the i we want
// we walk the members for our given location, and we increment some reference
// every time we see a positional if we go past all of the members and we were
// unable to do anything with that argument: we will complain later depending on
// meta_type

// Returned to describe that a member was already satisfied, or was just
// satisfied (ex, positionals)
struct Satisfied {};

// Helper types for disambiguating between flag and positional parse errors
struct PositionalParseError : ParseError {};
struct FlagParseError : ParseError {};

using parse_member_ref_return_type =
    std::variant<Satisfied, UsageError, PositionalParseError, FlagParseError>;

// Determine the valid return types from MetaInfo<T> from a call to ParseArgs
template <class T>
using parse_args_return_type = std::conditional_t<
    MetaInfo<T>::extra_args_ok, std::variant<T, UsageError, ParseError>,
    std::variant<T, UsageError, ParseError, UnknownArgError>>;

template <class T>
  requires(is_parsable<std::remove_reference_t<decltype(T::field_ref)>> ||
           std::is_convertible_v<decltype(T::field_ref), Options>)
parse_member_ref_return_type
try_parse_member_ref(T &member_ref, auto const &options_map,
                     int &positional_count, int &current_pos_index, auto &begin,
                     auto const end) {
  if constexpr (std::is_convertible_v<decltype(member_ref.field_ref),
                                      Options>) {
    // Do nothing.
    // Don't consume any arguments
    // Don't return an error
    // Silently pass without touching the member_ref.field_ref
    return Satisfied{};
  } else {
    // TODO: We could consider silently skipping members that don't have a
    // conversion
    static_assert(
        is_parsable<std::remove_reference_t<decltype(member_ref.field_ref)>>,
        "Can only parse members that are of types that we know how to convert! "
        "Consider specializing ArgParse!");
    using parse_type =
        ArgParse<std::remove_reference_t<decltype(member_ref.field_ref)>>;

    // Check to see if we have Options available for this member
    auto itr = options_map.find(member_ref.name);
    auto const options = itr != options_map.end() ? itr->second : Options{};
    // Check to see if we should be positional or a flag
    if (options.positional) {
      // If we are a positional flag, we are REQUIRED!
      // If the positional we are trying to decode is not the same index as this
      // one, we were satisfied earlier Every time we see a positional flag, we
      // increment our index
      if (current_pos_index++ < positional_count) {
        return Satisfied{};
      } else if (begin == end) {
        // If we reached the end of our arguments but we weren't satisfied, we
        // should report an error
        // TODO: Error for missing positionals (expected current_pos_index + 1
        // but have positional_count)
        return UsageError{};
      }
      // Try to parse this member as a positional
      auto local_begin = begin;
      auto parse_result = parse_type::Parse(local_begin, end);
      if (parse_result.index() >= 1) {
        // TODO: return errors better than this
        return PositionalParseError(std::get<1>(parse_result));
      }
      // Otherwise, we decoded the positional!
      // Write the reference
      member_ref.field_ref = std::get<0>(parse_result);
      // Then move past what we consumed and increment our number of decoded
      // positionals
      begin = local_begin;
      positional_count++;
      return Satisfied{};
    } else {
      if (begin == end) {
        // If we are at the end, all of the flags that we have seen are
        // satisfied EXCEPT required flags we have not yet seen! Cannot just
        // count (since multiflags exist)
        // TODO ^
        return Satisfied{};
      }

      std::string_view argstr(*begin);
      // Handle a flag
      // look for the resolved name (-- from flag, empty means not replaced)
      std::string search_name = options.name.empty()
                                    ? std::string("--") + member_ref.name.data()
                                    : options.name;
      if (search_name == argstr) {
        // The flag we are looking for matches! Lets try to parse it now, and
        // assign it to the ref We want to mutate a local begin, so that on
        // error we can try other types before giving up
        // TODO: We need to NOT use begin here, but rather begin + 1
        // (or if there is an = in argstr, split on that and use the rhs + the
        // begin + 1)
        auto local_begin = begin + 1;
        auto parse_result = parse_type::Parse(local_begin, end);
        if (parse_result.index() >= 1) {
          // TODO: return errors better than this
          return FlagParseError(std::get<1>(parse_result));
        }
        // T is always slot 0 of the parse result
        member_ref.field_ref = std::get<0>(parse_result);
        // If we succeeded, move past the things we consumed.
        begin = local_begin;
      } else if (argstr.starts_with(search_name + "=")) {
        // TODO: Handle the = case
        assert(false);
      }
      // If this arg does not match our flag, we are "satisfied" because there
      // was no error.
      return Satisfied{};
    }
  }
}

} // namespace detail

template <class T, class... TArgs>
detail::parse_args_return_type<T> ParseArgs(int argc, const char **argv,
                                            TArgs &&...args) {
  using meta_type = MetaInfo<T>;
  detail::parse_args_return_type<T> val(std::in_place_type_t<T>{},
                                        std::forward<TArgs>(args)...);
  using pack_type = std::remove_reference_t<std::remove_const_t<
      decltype(detail::do_on_bindings(std::get<0>(val), [](auto) {}))>>;
  using unpackable_error_type = detail::UnpackableWrapperError<
      T, detail::is_unpackable<pack_type, T, decltype([](auto) {})>,
      pack_type::size>;
  static_assert(unpackable_error_type::value,
                "Must have fewer members to be unpackable!");
  // Only start looking for arguments/options after the program name
  auto begin = argv + 1;
  auto const end = argv + argc;

  int positionals_decoded = 0;

  // Get our options map: member name --> Option if it exists
  std::unordered_map<std::string_view, Options> options_map;
  detail::do_on_bindings(std::get<T>(val), [&options_map](auto &&members) {
    options_map = detail::get_arg_options<T>(members);
  });
  int positionals_count = 0;
  for (auto const &[name, option] : options_map) {
    if (option.positional) {
      positionals_count++;
    }
  }

  while (begin != end) {
    // Check to see if we have help first
    if (detail::is_help<T>(*begin)) {
      detail::display_help(std::get<T>(val), argv[0]);
      return UsageError{};
    }
    // Now try to see if we have any members that match, or if we don't,
    // if we have any positionals we would parse here
    detail::do_on_bindings(
        std::get<T>(val),
        [&val, &begin, end, &options_map = std::as_const(options_map),
         &positionals_decoded](auto &&members) {
          // Walk each of the members and attempt to parse the values at begin
          auto old_begin = begin;
          int num_positionals = 0;
          auto results = std::apply(
              [&begin, end, &options_map, &positionals_decoded,
               &num_positionals](auto &...member_refs) {
                // Used for counting the number of positionals we have in our
                // members to match
                return std::array{detail::try_parse_member_ref(
                    member_refs, options_map, positionals_decoded,
                    num_positionals, begin, end)...};
              },
              members);
          // For our results, if our begin pointer hasn't moved, we ONLY have
          // failed if:
          // 1. we DID have a flag/positional there but we couldn't parse it
          // 2. we encountered something we don't know how to parse and we
          // have passed all of our positional args
          if (old_begin == begin) {
            // Error reporting precedence:
            // 1. UsageError
            // 2. FlagParseError
            // 3. PositionalParseError
            // 4. All satisfied, unknown arg
            detail::FlagParseError const *flag_error{};
            detail::PositionalParseError const *pos_error{};
            for (auto const &res : results) {
              if (auto const *usage = std::get_if<UsageError>(&res)) {
                // If we ever see a UsageError, exit immediately
                val.template emplace<UsageError>(*usage);
                return;
              } else if (auto const *fl =
                             std::get_if<detail::FlagParseError>(&res)) {
                // assert that we don't have a flag error. We should never
                // have two flags fail
                assert(flag_error == nullptr);
                flag_error = fl;
              } else if (auto const *p =
                             std::get_if<detail::PositionalParseError>(&res)) {
                // assert that we don't have a positional error. We should
                // never have two pos flags fail
                assert(pos_error == nullptr);
                pos_error = p;
              }
            }
            if (flag_error) {
              val.template emplace<ParseError>(*flag_error);
              return;
            }
            if (pos_error) {
              val.template emplace<ParseError>(*pos_error);
              return;
            }
            // Finally, if we disallow unknown args, handle that here
            if constexpr (!meta_type::extra_args_ok) {
              // TODO: Fill this out
              val.template emplace<UnknownArgError>();
              return;
            } else {
              // If we support extra args that we don't know about, skip this
              // by moving begin
              begin++;
            }
          } else {
            // If begin moved at all, we had to have at least some case of no
            // error
            // TODO: Debug log
          }
        });
    if (val.index() != 0) {
      // If we have an error to report, lets stop parsing the rest
      return val;
    }
  }
  // If begin == end here, AND we didn't decode our positionals
  // TODO: or our required flags
  // we error out here
  if (positionals_decoded < positionals_count) {
    val.template emplace<UsageError>();
  }
  return val;
}