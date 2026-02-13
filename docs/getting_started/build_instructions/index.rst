Build Instructions
==================

Select your operating system below to view specific build steps.

.. grid:: 2
    :gutter: 3

    .. grid-item-card:: Linux
       :link: linux
       :link-type: doc

       Build instructions for Ubuntu, Fedora, and Arch Linux using GCC/Clang via the command line.

    .. grid-item-card:: macOS
       :link: macos
       :link-type: doc

       Instructions for Intel and Apple Silicon Macs using Xcode or Homebrew dependencies.

.. grid:: 2
    :gutter: 3

    .. grid-item-card:: Windows (PowerShell)
       :link: windows
       :link-type: doc

       Standard command-line build using CMake and Ninja/MSBuild.

    .. grid-item-card:: Windows (Visual Studio)
       :link: visual-studio
       :link-type: doc

       Setup build and debug inside the Visual Studio IDE.

          

.. grid:: 2
    :gutter: 3

    .. grid-item-card:: Build Options
       :link: options
       :link-type: doc

       Reference for CMake flags (e.g., ``HUIRA_TESTS``) available on all platforms.

    .. grid-item-card:: Python Bindings
       :link: python-bindings
       :link-type: doc

       Installing the python bindings

.. toctree::
   :hidden:
   :maxdepth: 1

   linux
   macos
   windows
   visual-studio
   options
   python-bindings
