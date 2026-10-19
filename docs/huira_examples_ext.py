"""
huira_examples_ext.py
=====================
Sphinx extension that copies the example READMEs from ``examples/`` into the
docs tree before the build starts, so that ``docs.huira.space`` carries the same
walkthroughs as GitHub without maintaining two copies.

For each ``examples/NN_name/README.md`` it writes ``docs/examples/NN_name.md``,
and for ``examples/README.md`` it writes ``docs/examples/index.md``. On the way
in it:

* rewrites the handful of relative links so they resolve from inside the docs
  tree (``../README.md#c`` -> ``index.md#c``, ``../../docs/...`` -> ``../...``,
  ``../NN_name/`` links between examples -> ``NN_name.md``, and the
  ``NN_name/`` folder links in the index table -> ``NN_name.md``),
* appends a ``Source`` section to every example page with the Python and C++
  files side by side in tabs (via ``literalinclude``), plus a link to the folder
  on GitHub,
* adds a hidden toctree to the index so the pages appear in the sidebar.

The generated ``docs/examples/`` directory is build output: add it to
``.gitignore``.

Configuration (conf.py)
-----------------------
::

    extensions = ['sphinx_design', 'myst_parser', 'huira_examples_ext']

    # Optional (defaults shown). Paths are relative to conf.py.
    examples_source_dir = '../examples'
    examples_output_dir = 'examples'
    examples_github_url = 'https://github.com/huira-render/huira/tree/main/examples'
"""

from __future__ import annotations

import re
import shutil
from pathlib import Path

from sphinx.application import Sphinx
from sphinx.util import logging

logger = logging.getLogger(__name__)

EXAMPLE_DIR_RE = re.compile(r"^\d{2}_[a-z0-9_]+$")


def _rewrite_common(text: str) -> str:
    # Links into the docs tree: examples/ is one level below the repo root,
    # docs/examples/ is one level below docs/, so ../../docs/X -> ../X and
    # ../docs/X -> ../X.
    text = re.sub(r"\]\((?:\.\./)+docs/", "](../", text)
    return text


def _rewrite_example(text: str) -> str:
    text = _rewrite_common(text)
    # "Running the examples" link back to the examples index.
    text = text.replace("](../README.md", "](index.md")
    # Links to sibling examples: ](../03_load_a_model/) -> ](03_load_a_model.md)
    text = re.sub(r"\]\(\.\./(\d{2}_[a-z0-9_]+)/?(#[^)]*)?\)", r"](\1.md\2)", text)
    return text


def _rewrite_index(text: str) -> str:
    text = _rewrite_common(text)
    # Table links to example folders: ](01_first_render/) -> ](01_first_render.md)
    text = re.sub(r"\]\((\d{2}_[a-z0-9_]+)/?\)", r"](\1.md)", text)
    return text


def _source_section(name: str, py: Path | None, cpp: Path | None,
                    rel_prefix: str, github_url: str) -> str:
    """Build the MyST 'Source' section with sync'd tabs."""
    tabs = []
    if py is not None:
        tabs.append(
            f"````{{tab-item}} Python\n"
            f":sync: python\n\n"
            f"```{{literalinclude}} {rel_prefix}/{name}/{py.name}\n"
            f":language: python\n"
            f"```\n"
            f"````"
        )
    if cpp is not None:
        tabs.append(
            f"````{{tab-item}} C++\n"
            f":sync: cpp\n\n"
            f"```{{literalinclude}} {rel_prefix}/{name}/{cpp.name}\n"
            f":language: cpp\n"
            f"```\n"
            f"````"
        )
    if not tabs:
        return ""
    body = "\n".join(tabs)
    return (
        "\n\n## Source\n\n"
        f"The complete example, in both languages. "
        f"[View this example on GitHub]({github_url}/{name}).\n\n"
        f"`````{{tab-set}}\n{body}\n`````\n"
    )


def _generate(app: Sphinx) -> None:
    srcdir = Path(app.srcdir)
    src = (srcdir / app.config.examples_source_dir).resolve()
    out = srcdir / app.config.examples_output_dir
    github_url = app.config.examples_github_url.rstrip("/")

    if not src.is_dir():
        logger.warning("huira_examples_ext: examples dir not found: %s", src)
        return

    # literalinclude paths are relative to the generated page's directory.
    rel_prefix = Path(*([".."] * len(Path(app.config.examples_output_dir).parts)),
                      app.config.examples_source_dir).as_posix()

    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    names = sorted(
        d.name for d in src.iterdir()
        if d.is_dir() and EXAMPLE_DIR_RE.match(d.name) and (d / "README.md").is_file()
    )

    for name in names:
        folder = src / name
        readme = (folder / "README.md").read_text(encoding="utf-8")
        py = next(iter(sorted(folder.glob("*.py"))), None)
        cpp = next(iter(sorted(folder.glob("*.cpp"))), None)
        page = _rewrite_example(readme).rstrip()
        page += _source_section(name, py, cpp, rel_prefix, github_url)
        (out / f"{name}.md").write_text(page + "\n", encoding="utf-8")

    index_src = src / "README.md"
    index = index_src.read_text(encoding="utf-8") if index_src.is_file() else "# Examples\n"
    index = _rewrite_index(index).rstrip()
    toc = "\n".join(names)
    index += (
        "\n\n```{toctree}\n"
        ":hidden:\n"
        ":maxdepth: 1\n\n"
        f"{toc}\n"
        "```\n"
    )
    (out / "index.md").write_text(index, encoding="utf-8")

    logger.info("huira_examples_ext: generated %d example pages in %s", len(names), out)


def setup(app: Sphinx) -> dict:
    app.add_config_value("examples_source_dir", "../examples", "env")
    app.add_config_value("examples_output_dir", "examples", "env")
    app.add_config_value(
        "examples_github_url",
        "https://github.com/huira-render/huira/tree/main/examples",
        "env",
    )
    # Generate before Sphinx scans the source tree for documents.
    app.connect("config-inited", lambda app, config: _generate(app))
    return {"version": "1.0", "parallel_read_safe": True, "parallel_write_safe": True}
