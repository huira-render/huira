from pathlib import Path

from ._huira import *
from ._huira import __version__

_data_dir = Path(__file__).parent / "data"
if _data_dir.is_dir():
    set_data_dir(str(_data_dir))
