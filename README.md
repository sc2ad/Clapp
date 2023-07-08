# Clapp

In hopes of getting closer to Clap, but in C++.

## Motivating (?) Example

```cpp
struct Rename {
  int flag;
  Options __flag{.name = "--new-flag"};
};
int main(int argc, const char** argv) {
  auto result = ParseArgs<Rename>(argc, argv);
  assert(result.index() == 0);
  printf("Result is: %d\n", std::get<Rename>(result).flag);
}
// When called as: ./test --new-flag 10:
// Result is: 10
```

See the `tests/` folder for more examples.

## Why?

I wanted to see if I could make a CLI library where the end-user does not need to use any macros while still maintaining the flexibility of expression. There were no libraries out there that fit this description (at least, not without static reflection), so I decided to make one myself.

## How?

We perform reflection on the type passed in as a template via clang's `__builtin_dump_struct` extension. Couple this with some under-the-hood macros to perform structured binding in a SFINAE way, we can get a tuple of all (only up to 64) of the member names and references to their actual values.
Then all is left is allowing for some configuration, which we do through having a member that has a type convertible to `Options` and is prefixed with the name `__` that will be used for the specified member name.

Of course, we also want to provide the various specializations for allowing user parsable types, so we have an `ArgParse` template for that as well.

## Requirements/Usage

Because of the fact that we use `__builtin_dump_struct`, we are limited to `clang` for compilation. We also need at least clang 15, for both C++20 and for some of the shortcuts this library does.
Assuming you can use clang, however, this library is header only (except for the tests) so you can simply add a `-I` to the include folder and `#include <clapp/clapp.hpp>` and be on your merry way.

## Building the tests

```bash
CXX=clang++ CC=clang meson setup build
meson test -C build
# or 'ninja -C build test'
```
