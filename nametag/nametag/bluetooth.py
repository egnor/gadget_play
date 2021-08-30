# Hardware access to the nametag via bluepy (see protocol.py for encoding)

import contextlib
import logging
import time
from collections.abc import Iterable
from typing import Dict, List, NamedTuple, Optional, Set, Tuple

import bluepy.btle  # type: ignore

logger = logging.getLogger(__name__)

SendExpectPairs = Iterable[Tuple[bytes, bytes]]


class BluetoothError(Exception):
    pass


class Connection:
    def __init__(self, peripheral: bluepy.btle.Peripheral):
        try:
            self._peripheral = peripheral
            self.address = peripheral.addr
            service = peripheral.getServiceByUUID(0xFFF0)
            chars = service.getCharacteristics(0xFFF1)
            if len(chars) != 1:
                message = "Found {len(chars)} FFF0.FFF1 characteristics"
                raise BluetoothError(message)

            self._char = chars[0]
            descriptors = self._char.getDescriptors(forUUID=0x2902)
            if len(descriptors) != 1:
                message = "Found {len(descriptors)} CCCD entries for FFF0.FFF1"
                raise BluetoothError(message)

            handle = self._char.getHandle()
            self._listener = _Listener(address=self.address, handle=handle)
            self._peripheral.withDelegate(self._listener)
            descriptors[0].write(b"\1\0")  # enable notifications

            logger.info(f"({self.address}) Nametag ready!")
        except bluepy.btle.BTLEException as e:
            raise BluetoothError(str(e))

    def send_and_expect(self, *, pairs: SendExpectPairs, timeout=1.0):
        prefix = f"({self.address})\n      "
        for send, expect in pairs:
            if send:
                logger.debug(f"{prefix}Sending: {send.hex(' ')}")
                self._listener.received.clear()
                try:
                    self._char.write(send)
                except bluepy.btle.BTLEException as e:
                    raise BluetoothError(str(e))
            if expect:
                start_time = time.monotonic()
                expect_text = "[*]" if expect == b"*" else expect.hex(" ")
                while expect not in self._listener.received:
                    elapsed = time.monotonic() - start_time
                    if elapsed > timeout:
                        message = f"Never got expected reply: {expect_text}"
                        raise BluetoothError(message)
                    logger.debug(f"{prefix}-- Expect: {expect_text}")
                    try:
                        self._peripheral.waitForNotifications(timeout - elapsed)
                    except bluepy.btle.BTLEException as e:
                        raise BluetoothError(str(e))

                logger.debug(f"{prefix}>> Fulfil: {expect_text}")


class _Listener(bluepy.btle.DefaultDelegate):
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


class ScanDevice(NamedTuple):
    address: str
    code: str
    rssi: int


def scan(*, interface: int = 0) -> List[ScanDevice]:
    try:
        out: List[ScanDevice] = []
        scanner = bluepy.btle.Scanner(iface=interface)
        for dev in scanner.scan(timeout=2):
            try:
                name = next(v for t, d, v in dev.getScanData() if t == 9)
                mfgr = next(v for t, d, v in dev.getScanData() if t == 255)
            except StopIteration:
                continue

            if name == "CoolLED" and mfgr[-8:] == "0222ffff":
                code = (mfgr[2:4] + mfgr[:2]).upper()
                dev = ScanDevice(address=dev.addr, code=code, rssi=dev.rssi)
                out.append(dev)

        return out

    except bluepy.btle.BTLEException as e:
        raise BluetoothError(str(e))


@contextlib.contextmanager
def connection(
    *,
    address: Optional[str] = None,
    code: Optional[str] = None,
    interface: int = 0,
):
    if not address:
        logging.debug("Scanning for nametags...")
        devices = scan()
        logging.debug(
            f"Found {len(devices)} nametags"
            + "".join(
                f"\n      {d.code} ({d.address}) rssi={d.rssi}" for d in devices
            )
        )
        if not devices:
            raise BluetoothError("No nametags found")

        code = code and code.upper()
        matching = [d for d in devices if (not code) or d.code == code]
        if not matching:
            raise BluetoothError(f"No nametags match (code={code})")
        if len(matching) > 1:
            raise BluetoothError(
                f"Multiple nametags matched:\n"
                + "\n".join(f"  {d.code} ({d.address})" for d in matching)
            )

        address = matching[0].address

    try:
        logging.debug(f"({address}) Connecting to nametag...")
        with bluepy.btle.Peripheral(address, iface=interface) as peripheral:
            logging.debug(f"({peripheral.addr}) Connected to nametag")
            yield Connection(peripheral)
            logging.debug(f"({peripheral.addr}) Disconnecting from nametag...")
    except bluepy.btle.BTLEException as e:
        raise BluetoothError(str(e))


class RetryConnection:
    """Wrapper for Connection that reconnects on Bluetooth errors."""

    def __init__(self, *, exit_stack, timeout=None, **kwargs):
        self._exit_stack = exit_stack
        self._timeout = timeout
        self._connect_kwargs = kwargs
        self._connection = None
        try:
            self._connect_if_necessary()
        except BluetoothError as e:
            exc = f"\n{e}".replace("\n", "\n      ")
            logging.warn(f"Initial Bluetooth failure, will retry:{exc}")

    def send_and_expect(self, *, pairs: Iterable[Tuple[bytes, bytes]]):
        start_time = time.monotonic()
        pairs = list(pairs)
        while True:
            try:
                self._connect_if_necessary()
                self._connection.send_and_expect(pairs=pairs)
                return
            except BluetoothError as e:
                self._connection = None
                self._exit_stack.close()
                elapsed = time.monotonic() - start_time
                if self._timeout and elapsed > self._timeout:
                    timeout = f"{elapsed}s > {self._timeout}s timeout"
                    logging.warn(f"Bluetooth failure, giving up ({timeout})")
                    raise e
                exc = f"\n{e}".replace("\n", "\n      ")
                logging.warn(f"Bluetooth failure, will retry:{exc}")

    def _connect_if_necessary(self):
        if not self._connection:
            opener = connection(**self._connect_kwargs)
            self._connection = self._exit_stack.enter_context(opener)
            self._connect_kwargs["address"] = self._connection.address


@contextlib.contextmanager
def retry_connection(**kwargs):
    with contextlib.ExitStack() as exit_stack:
        yield RetryConnection(exit_stack=exit_stack, **kwargs)
