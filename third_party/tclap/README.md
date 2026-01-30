# TCLAP (Templatized C++ Command Line Parser)

- **Version:** 1.2.5
- **Source:** https://sourceforge.net/projects/tclap/
- **License:** MIT (see COPYING)

Vendored headers only. No modifications from upstream.

## Why vendored?

TCLAP is not available via conda-forge on Windows (see https://github.com/conda-forge/tclap-feedstock/issues/9). 
Since it's header-only, vendoring is straightforward. If conda-forge adds Windows support, 
we can remove this and return to the package manager.