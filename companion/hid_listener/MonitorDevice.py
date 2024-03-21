
import json
from pathlib import Path
from typing import Iterable

from .site_packages import hid
from .observers import Observer

from .Console import Console
from .Device import Device

THIS_FOLDER = Path(__file__).parent
config_file = THIS_FOLDER / "../../../sequence_transform_config.json"
config = json.load(open(config_file, 'rt', encoding="utf-8"))


class MonitorDevice:
    def __init__(self, hid_device: Device, observers: Iterable[Observer] = ()):
        self.hid_device = hid_device
        self.device = hid.Device(path=hid_device["path"])
        self.observers = observers
        self.current_line = ""

        Console.print_connect(hid_device)

    def read(self, size, encoding="utf-8", timeout=1) -> str:
        """Read size bytes from the device."""
        return self.device.read(size, timeout).decode(encoding)

    def read_line(self) -> str:
        """Read from the device's console until we get a \n."""
        while "\n" not in self.current_line:
            self.current_line += self.read(32).replace("\x00", "")

        line, self.current_line = self.current_line.split("\n", 1)
        return self.ascii_to_uft8(line)

    def read_message(self):
        message = self.read_line()

        for observer in self.observers:
            observer.notify(message)

    def run_forever(self):
        while True:
            try:
                self.read_message()

            except hid.HIDException:
                break

    @staticmethod
    def ascii_to_uft8(line: str) -> str:
        for token, ascii_char in config["sequence_token_symbols"].items():
            line = line.replace(ascii_char, token)

        for token, ascii_char in config["wordbreak_symbol"].items():
            line = line.replace(ascii_char, token)

        return line
