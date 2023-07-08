#include "clapp/clapp.hpp"
#include "clapp/types.hpp"
#include "gtest/gtest.h"
#include <array>
#include <string_view>

#include <variant>

struct SuperSimple {
  int flag;
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
  EXPECT_TRUE(std::holds_alternative<UnknownArgError>(v));
}

TEST(Simple, MissingFlag) {
  std::array args{"filename", "--flag"};
  auto v = ParseArgs<SuperSimple>(args.size(), args.data());
  EXPECT_TRUE(std::holds_alternative<ParseError>(v));
}

TEST(Simple, BadFlag) {
  std::array args{"filename", "--flag", "not an int"};
  auto v = ParseArgs<SuperSimple>(args.size(), args.data());
  EXPECT_TRUE(std::holds_alternative<ParseError>(v));
}

struct Rename {
  int flag;
  Options __flag{.name = "--new-flag"};
};

TEST(Simple, Rename) {
  std::array args{"filename", "--new-flag", "10"};
  auto v = ParseArgs<Rename>(args.size(), args.data());
  EXPECT_EQ(v.index(), 0);
  EXPECT_EQ(std::get<Rename>(v).flag, 10);
}

struct Positional {
  int positional;
  Options __positional{.positional = true};
};

TEST(Simple, Positional) {
  std::array args{"filename", "10"};
  auto v = ParseArgs<Positional>(args.size(), args.data());
  EXPECT_EQ(v.index(), 0);
  EXPECT_EQ(std::get<Positional>(v).positional, 10);
}

TEST(Simple, MissingPositional) {
  std::array args{"filename"};
  auto v = ParseArgs<Positional>(args.size(), args.data());
  EXPECT_TRUE(std::holds_alternative<UsageError>(v));
}

TEST(Simple, BadPositional) {
  std::array args{"filename", "notanint"};
  auto v = ParseArgs<Positional>(args.size(), args.data());
  EXPECT_TRUE(std::holds_alternative<ParseError>(v));
}

TEST(Simple, Help) {
  std::array args{"filename", "--help"};
  testing::internal::CaptureStdout();
  ParseArgs<Positional>(args.size(), args.data());
  auto out = testing::internal::GetCapturedStdout();
  EXPECT_EQ(out, "Usage: filename <positional>\n");
}
