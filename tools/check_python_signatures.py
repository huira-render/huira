"""Check that the Python bindings' signatures name Python classes, not C++ types.

pybind11 writes a function's signature, for help() and the API docs, when the function is bound.
A class bound after the function, or not bound at all, is named by its C++ type
(huira::Image<huira::SpectralBins<...>>): the first only reads badly, but a function that takes
or returns a type that is never bound can't be called at all. This lists every such signature,
in the huira package and its spectral submodules, and exits with 1 if there are any.

Run with the built package importable:

    python tools/check_python_signatures.py
"""

import inspect
import re
import sys

import huira
import huira.rgb
import huira.swir8
import huira.visible8

# A C++ type in a signature: pybind11 writes it fully qualified (huira::..., std::...).
CPP_TYPE = re.compile(r"\b[A-Za-z_]\w*::\w")


def signatures(function):
    """The signature lines of a pybind11 function's docstring: one, or one per overload."""
    lines = (function.__doc__ or "").splitlines()
    return [line.strip() for line in lines if "(" in line and ("->" in line or "self" in line)]


def members(module):
    """Every function, method and property of a module, with a name for it."""
    for name, obj in vars(module).items():
        if inspect.isclass(obj) and obj.__module__.startswith("huira"):
            for attr, member in vars(obj).items():
                if isinstance(member, property):
                    for accessor in (member.fget, member.fset):
                        if accessor is not None:
                            yield f"{module.__name__}.{name}.{attr}", accessor
                elif callable(member) or isinstance(member, (staticmethod, classmethod)):
                    yield f"{module.__name__}.{name}.{attr}", getattr(obj, attr)
        elif callable(obj) and not inspect.isclass(obj):
            yield f"{module.__name__}.{name}", obj


def main():
    found = []
    for module in (huira._huira, huira._huira.rgb, huira._huira.visible8, huira._huira.swir8):
        for name, function in members(module):
            for signature in signatures(function):
                if CPP_TYPE.search(signature):
                    found.append(f"{name}: {signature}")
    for line in sorted(set(found)):
        print(line)
    if found:
        print(f"\n{len(set(found))} signature(s) name C++ types: bind the classes they name "
              "before them (see bindings/python/src/bindings.hpp), or bind the types.")
        return 1
    print("Every signature names Python classes.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
