# Huira Build Options

This document explains the various options that can be set when building Huira with CMake.  These options allow you to customize the build process and enable or disable certain features of the library.  Please make sure you are familiar with the build process on your specific system before reading this document, as some options may require additional dependencies or configuration steps.

|       Option        | Default | Description |
| ------------------- | ------- | ----------- |
| [`HUIRA_TOOLS`](#huira_tools) | `ON`  | Build Huira's command line tools. |
| [`HUIRA_EXAMPLES`](#huira_examples)    | `OFF` | Build Huira's example applications. |
| [`HUIRA_TESTS`](#huira_tests)       | `OFF` | Build Huira's unit tests. |
| [`HUIRA_DOCS`](#huira_docs)        | `OFF` | Build Huira's documentation. This requires additional dependencies to be installed.  Please see below.|
| [`HUIRA_PYTHON`](#huira_python)      | `OFF` | Build Huira's Python bindings. |
| [`HUIRA_LOCAL_DEV`](#huira_local_dev)   | `ON`  | Enable development features such as warnings and assertions. |
| [`HUIRA_NATIVE_ARCH`](#huira_native_arch) | `ON`  | Enable architecture-specific optimizations for the host machine. |

## HUIRA_TOOLS

This option enables the `huira` command line tool for various, often required, operations (such as creating star catalogs for rendering star fields).  In general, this should always be built, and thus it is set to `ON`.

## HUIRA_EXAMPLES

This option enables building of a number of example applications that demonstrate how to use the library.  In general, these should only be built if you want to run the examples, and thus it is set to `OFF` by default.

## HUIRA_TESTS

This option enables the building of unit tests to ensure the correctness of the library.  In general, these should only be built if you want to run the tests, and thus it is set to `OFF` by default.  The tests are automatically run by our CI/CD pipeline and the results can be viewed on GitHub, so there is no reason to run them locally unless you are a developer.

## HUIRA_DOCS
Huira's documentation is available at [docs.huira.space](https://docs.huira.space/), but if you want to build the documentation locally, you can do so by installing the additional dependencies listed below, and setting `HUIRA_DOCS=ON` when configuring the build with CMake.

### Installing additional dependencies

From within the existing `huira_env` environment, run:

```bash
conda env update -f docs/docs-dependencies.yml
```

You can now build the documentation by configuring cmake with `HUIRA_DOCS=ON`

The HTML documentation will be be generated to `build/huira_docs/`

## HUIRA_PYTHON

In general, this setting should not be enabled manually as it will build the bindings but will be difficult to install/use.

The most recent version of huira's python bindings are available on PyPI, and can be installed with:
```
pip install huira
````

If you wish to build the python bindings from source, it is recommended to use `pip` directly by running:

```bash
pip install bindings/python/
```

## HUIRA_LOCAL_DEV

This option automatically adds any cmake files found in `local/` to the build.  This is useful for development purposes, as it allows you to easily add custom cmake files without modifying the main CMakeLists.txt file.  This is set to `ON` by default, but can be set to `OFF` if you want to disable this feature.

If you have not manually added any files to `local/` , then this option will have no effect.

## HUIRA_NATIVE_ARCH

This option enables architecture-specific optimizations for the host machine.  This can improve performance, but may also cause compatibility issues on some systems.  This is set to `ON` by default, but can be set to `OFF` if you want to disable this feature.  When distributing via PyPI and conda, this is set to `OFF` to ensure maximum compatibility across different systems.  If you are building from source on your own machine, it is recommended to leave this option enabled to take advantage of the performance benefits.
