## Description
Huira is a C++ library for rendering and ray-tracing for scientific applications.  Particularly spacecraft navigation and planetary science.

The project uses a limited selection of C++20, as it aims to be portable across macos, windows, and linux.

All includes should be in alphabetical order, with the STL includes first, followed by third-party libraries, and then project-specific includes. Each group should be separated by a blank line.

## File Structure
- This is a header-only library
- The include/huira directory contains the declarations in .hpp files
- The include/huira_impl directory contains the corresponding definitions in .ipp files (e.g., huira/foo.hpp includes huira_impl/foo.ipp)
- The .ipp files are meant to be included at the bottom of a corresponding .hpp file
- Treat .hpp as the public interface summary; .ipp holds implementation details and documentation

## Naming Conventions
- Use PascalCase for classes (e.g., `UserAccount`, `HttpRequestHandler`)
- Use snake_case for variables and functions (e.g., `user_count`, `calculate_total()`)
- Use trailing underscores for private members (e.g., `private_foo_`)
- Use SCREAMING_SNAKE_CASE for enums, constants, and macros (e.g., `MAX_BUFFER_SIZE`, `DEBUG_MODE`)

## Avoid
- `using namespace std;` in headers
- Magic numbers — use named constants
- C-style casts — use `static_cast`, `dynamic_cast`, etc.

## Documentation
- All public APIs must have Doxygen-style documentation
- Place documentation comments in .ipp files, not .hpp files
- Exceptions: one-liner implementations in .hpp, or declaration-specific notes that cannot reasonably be moved (e.g., template parameter constraints)
- Keep .hpp files clean and concise — they should read as a quick reference of the interface

## Namespaces
- All library code lives in the `huira` namespace
- sub-namespaces (e.g. `huira::detail` or `huira::units`) are allowed, but not required

## Python Bindings
- Python bindings are implemented in bindings/python/include/
- If the interface of a class or function is changed in C++, the corresponding Python bindings should be updated accordingly.

## Tests
- Tests should be implemented for as much of the public API as possible
- `tests/unittests/explicit_instantiations.cpp` should be updated as new classes are added for accurate code coverage.
