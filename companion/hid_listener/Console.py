
from abc import ABC
from typing import Any, Callable
from .Device import Device


class Console(ABC):
    @staticmethod
    def __print_connection(
        device: Device, text: str,
        color: Callable[[Any], str] = lambda x: x
    ) -> None:
        vendor = device["manufacturer_string"]
        product = device["product_string"]

        v_id = to_hex(device["vendor_id"])
        p_id = to_hex(device["product_id"])
        release = to_hex(device["release_number"])

        print(
            f"{text}" + color(f"{vendor} {product} ({v_id}:{p_id}:{release})")
        )

    @staticmethod
    def print_connect(device: Device) -> None:
        Console.__print_connection(
            device,
            cyan(f"HID console connected: "), cyan
        )

    @staticmethod
    def print_disconnect(device: Device) -> None:
        Console.__print_connection(
            device,
            cyan(f"HID console disconnected: "), cyan
        )

    @staticmethod
    def print_couldnt_connect(device: Device) -> None:
        Console.__print_connection(
            device,
            cyan(f"Could not connect to: "), cyan
        )


def to_hex(number: int) -> str:
    return "%04X" % number


def cyan(*text: Any) -> str:
    return f"\033[96m{' '.join(map(str, text))}\033[0m"
