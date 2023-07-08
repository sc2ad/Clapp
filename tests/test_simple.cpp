#include "clapp/clapp.hpp"
#include "clapp/types.hpp"
#include <array>
#include <string_view>

#include <gtest/gtest.h>

struct SuperSimple {
  int flag;
};

struct Rename {
  int flag;
  Options __flag{.name = "--new-flag"};
};

template <> struct MetaInfo<SuperSimple> {
  constexpr static std::string_view OptionsPrefix = "__";
  constexpr static std::array help_args{"--help", "--h"};
  constexpr static bool extra_args_ok = false;
};

TEST(Simple, SingleFlag) {
  std::array args{"filename", "--flag", "10"};
  auto v = ParseArgs<SuperSimple>(args.size(), args.data());
  EXPECT_EQ(v.index(), 0);
  EXPECT_EQ(std::get<SuperSimple>(v).flag, 10);
}

TEST(Simple, ExtraArg) {
  std::array args{"filename", "garbage", "--flag", "10"};
  auto v = ParseArgs<SuperSimple>(args.size(), args.data());
  EXPECT_EQ(v.index(), 3);
}

TEST(Simple, BadArg) {
  std::array args{"filename", "--flag"};
  auto v = ParseArgs<SuperSimple>(args.size(), args.data());
  EXPECT_EQ(v.index(), 2);
}

TEST(Simple, Rename) {
  std::array args{"filename", "--new-flag", "10"};
  auto v = ParseArgs<Rename>(args.size(), args.data());
  EXPECT_EQ(v.index(), 0);
  EXPECT_EQ(std::get<Rename>(v).flag, 10);
}