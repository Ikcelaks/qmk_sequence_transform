
from abc import ABC
from .Device import Device


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
