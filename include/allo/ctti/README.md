# allo ctti

This is a stripped down version of some dead code dredged up from [github.com/Manu343726/ctti](https://github.com/Manu343726/ctti)
Support for most C++ STL stuff is removed.

Although support for MSVC and GCC remain, this is only in theory. In practice
this is only tested with zig's clang distribution.

I believe this CTTI impl will sometimes suffer substitution failure, though it
has always worked for my purposes. I deleted some of the SFINAE stuff that was
in the original implementation because I didn't need it.
