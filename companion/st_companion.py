
from hid_listener import DeviceListener
from hid_listener.observers import PrintObserver


def main():
    """Acquire debugging information from usb hid devices"""
    print_observer = PrintObserver()

    device_finder = DeviceListener([
        print_observer,
    ])

    print("Looking for devices...", flush=True)
    device_finder.run_forever()


if __name__ == "__main__":
    main()
