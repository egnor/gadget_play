# Hardware access to the nametag via bluepy (see protocol.py for encoding)

import contextlib
import logging
import time
from collections.abc import Iterable
from typing import Dict, Optional, Set, Tuple

import bluepy.btle  # type: ignore

logger = logging.getLogger(__name__)


class ExpectFailure(bluepy.btle.BTLEException):
    pass


class Device:
    def __init__(self, peripheral: bluepy.btle.Peripheral):
        self._peripheral = peripheral
        self._service = peripheral.getServiceByUUID(0xFFF0)
        characteristics = self._service.getCharacteristics(0xFFF1)
        if len(characteristics) != 1:
            message = "Found {len(characteristics)} FFF0.FFF1 characteristics"
            raise bluepy.btle.BTLEException(message)

        self._characteristic = characteristics[0]
        descriptors = self._characteristic.getDescriptors(forUUID=0x2902)
        if len(descriptors) != 1:
            message = "Found {len(descriptors)} CCCD entries for FFF0.FFF1"
            raise bluepy.btle.BTLEException(message)

        addr = self._peripheral.addr
        handle = self._characteristic.getHandle()
        self._listener = _DeviceListener(address=addr, handle=handle)
        self._peripheral.withDelegate(self._listener)
        descriptors[0].write(b"\1\0")  # enable notifications
        logger.info(f"({addr}) Nametag ready!")

    def send_and_expect(self, *, pairs: Iterable[Tuple[bytes, bytes]]):
        addr = self._peripheral.addr
        for send, expect in pairs:
            logger.debug(f"({addr})\n      Sending: {send.hex(' ')}")
            if send:
                self._listener.received.clear()
                self._characteristic.write(send)
            if expect:
                expect_hex = "[*]" if expect == b"*" else expect.hex(" ")
                if expect in self._listener.received:
                    logger.debug(f"({addr})\n   >> Already: {expect_hex}")
                elif expect:
                    logger.debug(f"({addr})\n  ... Waiting: {expect_hex}")
                    self._peripheral.waitForNotifications(1.0)
                    if expect not in self._listener.received:
                        message = f"Never got expected reply: {expect_hex}"
                        raise ExpectFailure(message)


class _DeviceListener(bluepy.btle.DefaultDelegate):
    def __init__(self, *, address: str, handle: int):
        bluepy.btle.DefaultDelegate.__init__(self)
        self._address = address
        self._handle = handle
        self.received: Set[bytes] = set()

    def handleNotification(self, handle, data):
        addr = self._address
        if handle == self._handle:
            logger.debug(f"({addr})\n   => Receive: {data.hex(' ')}")
            self.received.add(data)
            self.received.add(b"*")  # Match wildcard expectations.
        else:
            logger.warn(f"({addr}) Unexpected notify handle #{handle}")


def scan_devices(*, interface: int = 0) -> Dict[str, str]:
    out: Dict[str, str] = {}
    scanner = bluepy.btle.Scanner(iface=interface)
    for dev in scanner.scan(timeout=2):
        try:
            name = next(v for t, d, v in dev.getScanData() if t == 9)
            mfgr = next(v for t, d, v in dev.getScanData() if t == 255)
        except StopIteration:
            continue

        if name == "CoolLED" and mfgr[-8:] == "0222ffff":
            out[(mfgr[2:4] + mfgr[:2]).upper()] = dev.addr

    return out


@contextlib.contextmanager
def device_open(
    *,
    address: Optional[str] = None,
    code: Optional[str] = None,
    interface: int = 0,
):
    if not address:
        logging.debug("Scanning for nametags...")
        devices = scan_devices()
        logging.debug(f"Found {len(devices)} nametags")
        if code:
            address = devices.get(code.upper())
            if not address:
                message = f'No device code "{code}" ({len(devices)} total)'
                raise bluepy.btle.BTLEException(message)
        elif not devices:
            raise bluepy.btle.BTLEException("No nametags found")
        elif len(devices) > 1:
            message = "Multiple nametags (address needed):\n" + "\n".join(
                f"  {k} ({v})" for k, v in devices.items()
            )
            raise bluepy.btle.BTLEException(message)
        else:
            address = next(iter(devices.values()))

    logging.debug(f"({address}) Connecting to nametag...")
    with bluepy.btle.Peripheral(address, iface=interface) as peripheral:
        logging.debug(f"({peripheral.addr}) Connected to nametag")
        yield Device(peripheral)
        logging.debug(f"({peripheral.addr}) Disconnecting from nametag...")


class RetryDevice:
    """Wrapper for Device that reconnects on Bluetooth errors."""

    def __init__(self, *, exit_stack, timeout=None, **kwargs):
        self._exit_stack = exit_stack
        self._timeout = timeout
        self._open_kwargs = kwargs
        self._device = None
        try:
            self._connect_if_necessary()
        except bluepy.btle.BTLEException as e:
            exc = f"\n{e}".replace("\n", "\n      ")
            logging.warn(f"Initial Bluetooth failure, will retry:{exc}")

    def send_and_expect(self, *, pairs: Iterable[Tuple[bytes, bytes]]):
        start_time = time.time()
        pairs = list(pairs)
        while True:
            try:
                self._connect_if_necessary()
                self._device.send_and_expect(pairs=pairs)
                return
            except bluepy.btle.BTLEException as e:
                self._device = None
                self._exit_stack.close()
                elapsed = time.time() - start_time
                if self._timeout and elapsed > self._timeout:
                    timeout = f"{elapsed}s > {self._timeout}s timeout"
                    logging.warn(f"Bluetooth failure, giving up ({timeout})")
                    raise
                exc = f"\n{e}".replace("\n", "\n      ")
                logging.warn(f"Bluetooth failure, will retry:{exc}")

    def _connect_if_necessary(self):
        if not self._device:
            opener = device_open(**self._open_kwargs)
            self._device = self._exit_stack.enter_context(opener)


@contextlib.contextmanager
def retry_device_open(**kwargs):
    with contextlib.ExitStack() as exit_stack:
        yield RetryDevice(exit_stack=exit_stack, **kwargs)
