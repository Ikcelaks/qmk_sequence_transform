
from threading import Thread
from typing import Iterable
from time import sleep

from .observers import Observer
from .site_packages import hid

from .MonitorDevice import MonitorDevice
from .Console import Console
from .Device import Device

USAGE_PAGE = 0xFF31
USAGE = 0x0074


class DeviceListener:
    def __init__(self, observers: Iterable[Observer] = ()):
        self.live_devices: dict[bytes, Device] = {}
        self.observers = observers

    def run_forever(self) -> None:
        """Process messages from our queue in a loop."""
        while True:
            try:
                self.remove_dead_devices()

                for device in self.find_devices():
                    self.register_device(device)

                sleep(0.1)

            except KeyboardInterrupt:
                break

    def remove_dead_devices(self) -> None:
        for key, device in list(self.live_devices.items()):
            if not device["thread"].is_alive():
                Console.print_disconnect(device)
                del self.live_devices[key]

    def register_device(self, device: Device) -> None:
        if device["path"] not in self.live_devices:
            self.live_devices[device["path"]] = device

            try:
                monitor = MonitorDevice(device, self.observers)

                device["thread"] = Thread(
                    target=monitor.run_forever, daemon=True
                )

                device["thread"].start()

            except Exception as e:
                device["e"] = e
                device["e_name"] = e.__class__.__name__

                Console.print_couldnt_connect(device)
                del self.live_devices[device["path"]]

    @staticmethod
    def is_console_hid(hid_device: Device) -> bool:
        """Returns true when the usage page indicates
        it"s a teensy-style console."""

        return (
            hid_device["usage_page"] == USAGE_PAGE and
            hid_device["usage"] == USAGE
        )

    def find_devices(self) -> list[Device]:
        """Returns a list of available teensy-style consoles.
        """
        hid_devices = hid.enumerate()
        devices: list[Device] = list(filter(self.is_console_hid, hid_devices))

        # Add index numbers
        device_index = {}

        for device in devices:
            device_id = f"{device['vendor_id']:04X}:{device['product_id']:04X}"

            if device_id not in device_index:
                device_index[device_id] = 0

            device_index[device_id] += 1
            device["index"] = device_index[device_id]

        return devices
