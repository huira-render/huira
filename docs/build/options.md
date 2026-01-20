# Huira Build Options

*Workin in progress*

## Building Documentation
To build Huira's documentation, you will need to install additional dependencies.  From within the existing `huira_env` environment, run:

```bash
conda env update -f docs/docs-dependencies.yml
```

One the dependencies are installed, build the documentation with:

```bash
mkdir build
cd build
cmake -DHUIRA_DOCS=ON ../
cmake --build . -j
```

The HTML documentation will be be generated to `build/huira_docs/`
