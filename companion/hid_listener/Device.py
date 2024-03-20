
from typing import TypedDict
from threading import Thread

from .site_packages import hid


class Device(TypedDict):
    vendor_id: int
    product_id: int
    serial_number: str
    release_number: int
    manufacturer_string: str
    product_string: str
    usage_page: int
    usage: int
    interface_number: int
    bus_type: hid.BusType
    index: int
    thread: Thread
    e: Exception
    e_name: str
    path: bytes
