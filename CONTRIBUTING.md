# Contributing to Huira

Thanks for your interest in improving Huira! This guide covers how to propose a change, what a
pull request should look like, the code and commit conventions, and how to disclose AI assistance.

## Before you start

- **Bug fixes, documentation and small improvements:** open a pull request directly.
- **New features, public API changes, large refactors and new dependencies:** open an issue first
  (use the *Feature Request* template) so we can agree on the approach before you write much code.
- **Questions:** use the *Question* issue template.

New dependencies need particular care. Each one has to build in the conda environment
(`packaging/environment.yml`), the conda recipe (`packaging/recipe/`), the vcpkg port
(`packaging/vcpkg-port/`) and the PyPI wheels, on Linux, macOS and Windows.

## Pull requests

### One change per pull request

A pull request should do one thing that you can describe in a sentence.

- A bug you find while working on something else gets its own pull request. Fixes shouldn't wait
  for features.
- Keep refactors (moving, renaming or restructuring code without changing behavior) separate from
  changes in behavior.
- Keep formatting-only changes in their own pull request.
- Put tests in the same pull request as the code they test.

Most pull requests should be a few hundred changed lines. If yours is heading past about 1,000
(not counting tests or mechanical changes such as renames), look for a way to split it, or open an
issue to plan the split.

### Keep `main` releasable

`main` must build and pass all tests after every merge, so that a release can be cut from it at any
time. That doesn't mean a feature has to be finished before any of it is merged. Large features
land as a series of pull requests:

1. **Preparation:** refactors that make the feature easy to add, with no change in behavior.
2. **Internals:** the new code, with tests, but not yet reachable from the public API.
3. **Exposure:** a final pull request that makes the feature public: adding it to
   `include/huira/huira.hpp` or the public handles, the Python bindings, the docs pages, and an
   example where it helps.

Unfinished code on `main` is fine as long as nothing public reaches it. If your next pull request
depends on one that's still open, branch from that one and write "Depends on #123" in the
description.

To change an existing public API, add the new version alongside the old one where you can, move
callers over, and remove the old one in a final pull request. Mark breaking changes as described in
[Commit messages](#commit-messages).

Please don't change the version in `VERSION` or `packaging/recipe/meta.yaml`. Versions, releases
and the README's feature list are updated by the maintainer when a release is cut.

### What every pull request needs

- **Tests.** New behavior comes with tests, and a bug fix comes with a test that fails without the
  fix. Unit tests use [Catch2](https://github.com/catchorg/Catch2) and live in
  `tests/unittests/huira/`, mirroring `include/huira/`. Add new test files to the `UNIT_TESTS` list
  in `tests/unittests/CMakeLists.txt`. New class templates also get an explicit instantiation in
  `tests/unittests/explicit_instantiations.cpp`, so that coverage includes all of their members.
- **Python bindings.** If you change a C++ interface that is exposed to Python, update its binding
  (the matching `*_py.ipp` under `bindings/python/include/`) in the same pull request. New bindings
  are registered in `bindings/python/src/huira_module.cpp`.
- **Documentation.** Public APIs need Doxygen comments (see
  [Documentation comments](#documentation-comments)). A new public class also needs a page under
  `docs/api_reference/cpp_api/`, and under `docs/api_reference/python_api/` if it is exposed to
  Python.
- **Verification.** For changes to rendering, radiometry or other numerical behavior, say in the
  pull request how you checked the result: reference values, analytical checks or comparison
  renders.

### Branches, review and merging

- Branch from `main`, and keep your branch current by rebasing onto `main` rather than merging
  `main` into it. Force-pushing your pull request branch after a rebase is fine
  (`git push --force-with-lease`).
- Open a draft pull request whenever you'd like early feedback.
- Address review comments by fixing the commit they apply to (`git commit --fixup=<commit>`, then
  `git rebase -i --autosquash main`) rather than adding "Address review" commits. Automated review
  comments, such as Copilot's, are treated like any other review: fix the issue or reply
  explaining why not.
- All CI checks must pass before merging.
- Pull requests are merged with **rebase and merge**, so each commit lands on `main` as you wrote it
  and should build and pass the tests on its own. A pull request whose history isn't clean may be
  squash-merged instead, with its title as the commit message, so write the title in the same
  format as a commit message.

## Commit messages

Huira uses [Conventional Commits](https://www.conventionalcommits.org/). A message starts with a
type, an optional scope and a short summary, followed by an optional body explaining what changed
and why:

```
fix: Write integer FITS images unsigned, at full range

write_image_fits() documented BITPIX 16 and 32 as unsigned, but created
signed images and clamped to the signed maximum, so a 16-bit sensor's
pixels saturated at 32767.

Create USHORT_IMG and ULONG_IMG so CFITSIO writes BZERO, and add
test_fits_io, whose cases all fail without this change.
```

- Write the summary in the imperative mood ("Add", "Fix", not "Added", "Fixes"), capitalized, with
  no trailing period.
- Types: `feat`, `fix`, `perf`, `refactor`, `docs`, `test`, `style`, `build`, `ci`, `chore`. A scope
  in parentheses is optional, as in `feat(python):`.
- Mark breaking changes with a `!` after the type (`refactor!: ...`) or a `BREAKING CHANGE:`
  trailer.

## Code style

### Formatting

Formatting is enforced in CI with `clang-format`, using the [`.clang-format`](.clang-format) file at
the root of the repository. Most IDEs (Visual Studio, VS Code, CLion) pick it up automatically.
From the command line:

```bash
pip install clang-format              # the same tool CI uses
clang-format -i path/to/your/file.cpp
```

To run the same check as CI, from the root of the repository:

```bash
find examples include apps tests bindings -type f \
    \( -name "*.cpp" -o -name "*.hpp" -o -name "*.cc" -o -name "*.h" -o -name "*.ipp" \) \
    -exec clang-format --style=file --dry-run --Werror {} +
```

The main rules:

- **Line length:** at most 100 characters.
- **Indentation:** 4 spaces, no tabs.
- **Braces:** classes, structs and control statements open their brace on the same line; function
  definitions open theirs on the next line. Single-statement `if` and `while` bodies always get
  braces.
- **Includes:** sorted and grouped automatically.

### Naming

- **Types** (classes, structs, enums) **and enumerators:** `PascalCase`, e.g. `WrapMode::Repeat`
- **Functions and variables:** `snake_case`
- **Constants:** `SCREAMING_SNAKE_CASE`, e.g. `DAYS_PER_JULIAN_YEAR`
- **Macros:** `SCREAMING_SNAKE_CASE` with a `HUIRA_` prefix
- **Private members:** `snake_case` with a trailing underscore, e.g. `distortion_`

### Code organization

- Huira is header-only. Declarations go in `include/huira/**/*.hpp` and definitions in the matching
  `include/huira_impl/**/*.ipp`, which is included at the bottom of its `.hpp`.
- All library code lives in the `huira` namespace. Sub-namespaces such as `huira::detail` are fine.
- Huira uses C++20, but only the parts that work with GCC, Clang and MSVC on Linux, macOS and
  Windows.
- Avoid `using namespace std;` in headers, magic numbers (use named constants) and C-style casts.

### Documentation comments

Public APIs need Doxygen comments. Document classes and structs in the `.hpp`, and functions and
methods in the `.ipp`, so that the `.hpp` reads as a quick reference to the interface. Short
one-line definitions in the `.hpp`, and notes that only make sense on the declaration (such as
template constraints), are the exception.

## Building and testing

See the build guides for [Linux](docs/getting_started/build_instructions/linux.md),
[macOS](docs/getting_started/build_instructions/macos.md),
[Windows](docs/getting_started/build_instructions/windows.md) and
[Visual Studio](docs/getting_started/build_instructions/visual-studio.md). With the conda
environment on Linux or macOS, building and running the tests looks like this:

```bash
conda env create -f packaging/environment.yml
conda activate huira_env

mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/conda-toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DHUIRA_TESTS=ON ../
cmake --build . -j
ctest --output-on-failure
```

On Windows with Visual Studio, run the tests with `ctest -C Release --output-on-failure`. CI runs
the Linux tests in a `Debug` build, which enables extra checks, so if a test passes for you but
fails in CI, try a `Debug` build.

To build and install the Python bindings from source, run `pip install bindings/python/` (see
[Python Bindings](docs/getting_started/build_instructions/python-bindings.md)). Other CMake options
are described in [Build Options](docs/getting_started/build_instructions/options.md).

## AI-Assisted Contributions

AI tools are allowed, but you remain responsible for understanding every line you submit, and
should be able to explain it in review.

If any part of a pull request was generated or substantially shaped by an AI tool, disclose it in
the pull request description, using the section provided in the template:

1. Check the **AI-assisted** box.
2. Name the model(s) used, including the version.
3. List the categories it was used for, from the table below.
4. If `algorithm` is listed, describe what the AI contributed and how you verified the result
   (e.g. reference values, comparison renders, analytical checks).

Commit messages don't need to mention AI use.

| Category | Use for |
| --- | --- |
| `docs` | Docstrings, comments, Markdown documentation |
| `tests` | Unit tests, test fixtures |
| `ci` | CI workflows |
| `build` | CMake, packaging, distribution |
| `boilerplate` | Repetitive or structural code (bindings, accessors, etc.) |
| `refactor` | Restructuring existing code without changing behavior |
| `algorithm` | Implementing or modifying numerical, physical, or rendering logic |
| `review` | Using AI to review or debug code you wrote |

Disclosure is not grounds for rejection; it helps reviewers know where to look more closely.

## License

By contributing, you agree that your contributions will be licensed under the
[MIT License](LICENSE).