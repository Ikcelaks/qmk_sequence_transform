
from typing import Protocol


class Observer(Protocol):
    @staticmethod
    def notify(message: str) -> None:
        ...
