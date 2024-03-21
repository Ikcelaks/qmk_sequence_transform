
from pathlib import Path
import ctypes

THIS_FOLDER = Path(__file__).parent
ctypes.CDLL(str(THIS_FOLDER / "../hidapi.dll"))
