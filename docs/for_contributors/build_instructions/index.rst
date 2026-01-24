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

.. grid:: 1
    :gutter: 3
    :padding: 3

    .. grid-item-card:: Windows Builds
       :class-header: bg-light

       Huira supports two workflows on Windows. Choose the one that matches your environment.

       .. grid:: 2
          :gutter: 3

          .. grid-item-card:: PowerShell
             :link: windows
             :link-type: doc
             :class-card: sd-border-0

             Standard command-line build using CMake and Ninja/MSBuild.

          .. grid-item-card:: Visual Studio
             :link: visual-studio
             :link-type: doc
             :class-card: sd-border-0

             Generate a ``.sln`` solution file to build and debug inside the Visual Studio IDE.

.. grid:: 1
    :gutter: 3

    .. grid-item-card:: Build Options
       :link: options
       :link-type: doc

       Reference for CMake flags (e.g., ``HUIRA_BUILD_TESTS``, ``HUIRA_USE_DOUBLE``) available on all platforms.

.. toctree::
   :hidden:
   :maxdepth: 1

   linux
   macos
   windows
   visual-studio
   options
