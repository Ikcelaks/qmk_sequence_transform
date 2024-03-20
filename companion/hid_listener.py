
from threading import Thread
from typing import TypedDict
from time import sleep
from abc import ABC

from message_observers import Observer

import ctypes
ctypes.CDLL("./hid_lib/hidapi.dll")

from hid_lib import hid  # noqa

USAGE_PAGE = 0xFF31
USAGE = 0x0074


class Device(TypedDict):
    vendor_id: int
    product_id: int
    serial_number: str
    release_number: int
    manufacturer_string: str
    usage_page: int
    usage: int
    interface_number: int
    bus_type: hid.BusType
    index: int
    thread: Thread
    e: Exception
    e_name: str
    path: bytes


class Console(ABC):
    @staticmethod
    def print_connect(device: Device) -> None:
        pass

    @staticmethod
    def print_disconnect(device: Device) -> None:
        pass

    @staticmethod
    def print_couldnt_connect(device: Device) -> None:
        pass


class MonitorDevice:
    def __init__(self, hid_device: Device, observers: tuple[Observer] = ()):
        self.hid_device = hid_device
        self.device = hid.Device(path=hid_device["path"])
        self.observers = observers
        self.current_line = ""

        Console.print_connect(hid_device)

    def read(self, size, encoding="ascii", timeout=1) -> str:
        """Read size bytes from the device."""
        return self.device.read(size, timeout).decode(encoding)

    def read_line(self) -> str:
        """Read from the device's console until we get a \n."""
        while "\n" not in self.current_line:
            self.current_line += self.read(32).replace("\x00", "")

        line, self.current_line = self.current_line.split("\n", 1)
        return line

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


class DeviceListener:
    def __init__(self, observers: tuple[Observer] = ()):
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
        for device in self.live_devices.values():
            if not device["thread"].is_alive():
                Console.print_disconnect(device)
                del device

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
