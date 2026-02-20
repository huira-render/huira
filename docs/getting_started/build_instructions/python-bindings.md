# Python Bindings

## Installing from PyPI
This is recommended unless you want to make modifications to the source code.  The PyPI package is built from the latest stable release, so it may not include the most recent commits.  If you want to use the latest code, please build from source using the instructions below.
```bash
pip install huira
```

## Building from Source

If you want to build the python bindings from source, please make sure that you have the necessary dependencies installed.  Check the C++ build instructions for your specific platform for more details on how to install the dependencies.

Once you have confirmed you have the necessary requirements, you can build and install the python bindings with:

```bash
pip install bindings/python/
```

You can verify the installation with:
```bash
python -c "import huira; print(huira.__version__)"
```

NOTE: Do not run `import huira` from within the `bindings/python/` directory.  This will cause an import error as it will try to import the unbuilt source files rather than the installed version.
