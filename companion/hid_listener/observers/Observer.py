
from typing import Protocol


class Observer(Protocol):
    def notify(self, message: str) -> None:
        ...
