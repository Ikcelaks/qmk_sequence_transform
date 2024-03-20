
from . import Observer


class PrintObserver(Observer):
    def notify(self, message) -> None:
        print(message)
