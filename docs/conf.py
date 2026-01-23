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
breathe_default_members = ('members', 'undoc-members')
breathe_show_include = False

html_theme = "furo"
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

# Furo theme options:
html_theme_options = {
    "light_css_variables": {
        "color-brand-primary": "#2980b9",
        "color-brand-content": "#2980b9",
    },
    "dark_css_variables": {
        "color-brand-primary": "#5ba3d0",
        "color-brand-content": "#5ba3d0",
    },
    "sidebar_hide_name": True,
}

