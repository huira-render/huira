from pathlib import Path

from ._huira import *
from ._huira import __version__

# The spectral submodules, as the wrapper modules huira.rgb, huira.visible8 and huira.swir8.
# `from ._huira import *` above binds each name to the compiled submodule, and importing a
# wrapper (import huira.rgb, or from huira.rgb import Scene) then replaces it, so which object
# huira.rgb was depended on what had been imported first. Importing the wrappers here, as
# absolute imports (a relative `from . import rgb` would take the attribute already bound and
# import nothing), binds each name to the wrapper from the start.
import huira.rgb  # noqa: E402
import huira.swir8  # noqa: E402
import huira.visible8  # noqa: E402

_data_dir = Path(__file__).parent / "data"
if _data_dir.is_dir():
    set_data_dir(str(_data_dir))
