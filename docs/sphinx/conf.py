project = "HyperBerry"
version = "0.0.1"
release = version
author = "HyperBerry Contributors"
copyright = "2024, HyperBerry Contributors"

extensions = ["breathe"]

breathe_projects = {"HyperBerry": "../_doxygen/xml"}
breathe_default_project = "HyperBerry"
breathe_default_options = {
    "members": None,
    "undoc-members": None,
    "private-members": None,
}

html_theme = "furo"
html_theme_options = {
    "sidebar_hide_name": False,
}

exclude_patterns = ["_build"]
