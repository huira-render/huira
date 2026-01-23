import os
import sys

# Get absolute path to this file
conf_dir = os.path.abspath(os.path.dirname(__file__))

# Project Name and Author:
project = 'huira'
copyright = '2025, Christopher Gnam'
author = 'Christopher Gnam'

# Required Extensions:
extensions = [
    'breathe',
    'sphinx.ext.autodoc',
    'sphinx.ext.autosummary',
    'sphinx.ext.napoleon',
    'sphinx.ext.viewcode',
    'sphinx.ext.githubpages',
    'sphinx.ext.mathjax',
    'sphinx.ext.autosectionlabel',
    'sphinx_rtd_theme',
    'sphinx_autodoc_typehints',
    'sphinx_toolbox.sidebar_links',
    'myst_parser'
]

# Documentation Options:
autodoc_default_options = {
    'members': True,
    'member-order': 'bysource',
    'special-members': '__init__',
    'undoc-members': True,
    'exclude-members': '__weakref__'
}

# Mock imports for any dependencies that might not be available during doc build
autodoc_mock_imports = []

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

myst_heading_anchors = 3
numfig = True
autosectionlabel_prefix_document = True

# Breathe Configuration
breathe_default_project = "huira"

html_theme = "sphinx_rtd_theme"
html_static_path = ['_static']
html_logo = '_static/logo/full/white_on_transparent.svg'

# Custom CSS overrides:
html_css_files = [
    'css/custom.css',
]

# Custom JavaScript:
html_js_files = [
    'js/logo_link.js',
]

# Add global toc tree to all html pages
html_sidebars = { '**': ['globaltoc.html', 'relations.html', 'sourcelink.html', 'searchbox.html'] }

# Custom RTD settings:
html_theme_options = {
    'collapse_navigation': False,
    'style_external_links': True,
    'titles_only': True
}

