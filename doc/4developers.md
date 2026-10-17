# 4 Developers
The very first thing to understand before writing any code is the fact that this library is designed for `C under posix standard`, which means that we cannot actually build something that is compatible with `RAII` in `C++` or ownership mechanism in `Rust` (which are strongly bound to other mechanism in those languages). Our best bet is to build something that can simplify the procedure of using heap memory.

## Pointer varieties

1. Local pointers
Large amount of pointers should be freed before the end of the function who called `allocate` procedure. Those pointers are bound to specific stack frames so we call them `local pointers`.
2. Thread pointers
`Local pointers` which escaped the function who called `allocate` procedure but yet not escaped from the threads who created them.
3. Global pointers (being used in different threads, are also thread pointers)


