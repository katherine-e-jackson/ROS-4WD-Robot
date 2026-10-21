"""
Real-time keyboard teleop for the ESP32 4WD robot over BLE.

Hold W/A/S/D to drive; release to stop. Press ESC or Q to quit

"""

import asyncio
import sys
from bleak import BleakClient, BleakScanner

DEVICE_NAME = "ESP32-Robot"

# Standard GATT services present on every BLE device — never the one we want.
STANDARD_SERVICE_PREFIXES = ("00001800", "00001801")

KEY_TO_COMMAND = {
    'w': b'w',
    's': b's',
    'a': b'a',
    'd': b'd',
}
STOP_COMMAND = b' '


class RobotTeleop:
    def __init__(self):
        self.client: BleakClient | None = None
        self.loop: asyncio.AbstractEventLoop | None = None
        self.active_key: str | None = None
        self.char_uuid: str | None = None

    async def connect(self):
        print(f"Scanning for '{DEVICE_NAME}'...")
        device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
        if device is None:
            raise RuntimeError(
                f"Could not find a device named '{DEVICE_NAME}'. "
                "Make sure the robot is powered on and not already connected elsewhere."
            )
        self.client = BleakClient(device)
        await self.client.connect()
        print("Connected. Looking for the command characteristic...")

        self.char_uuid = self._find_writable_char()
        if self.char_uuid is None:
            await self.client.disconnect()
            raise RuntimeError(
                "Connected, but couldn't find a writable custom characteristic. "
                "Check that the robot firmware's ble_control service is running."
            )
        print(f"Found characteristic: {self.char_uuid}")
        print("Hold W/A/S/D to drive, release to stop. ESC or Q to quit.")

    def _find_writable_char(self) -> str | None:
        for service in self.client.services:
            if service.uuid.replace("-", "").startswith(STANDARD_SERVICE_PREFIXES):
                continue
            for char in service.characteristics:
                if "write-without-response" in char.properties or "write" in char.properties:
                    return char.uuid
        return None

    async def send(self, data: bytes):
        if self.client and self.client.is_connected and self.char_uuid:
            try:
                await self.client.write_gatt_char(self.char_uuid, data, response=True)
            except Exception as e:
                print(f"Write failed: {e}")

    async def disconnect(self):
        if self.client and self.client.is_connected:
            await self.send(STOP_COMMAND)  # stop car before disconnecting
            await self.client.disconnect()
            print("Disconnected.")

    # --- Called from the pynput listener thread ---
    def on_press(self, key_char: str):
        if key_char in KEY_TO_COMMAND and self.active_key != key_char:
            self.active_key = key_char
            asyncio.run_coroutine_threadsafe(
                self.send(KEY_TO_COMMAND[key_char]), self.loop
            )

    def on_release(self, key_char: str):
        if key_char == self.active_key:
            self.active_key = None
            asyncio.run_coroutine_threadsafe(self.send(STOP_COMMAND), self.loop)


async def main():
    teleop = RobotTeleop()
    teleop.loop = asyncio.get_running_loop()

    try:
        await teleop.connect()
    except RuntimeError as e:
        print(e)
        sys.exit(1)

    # pynput listener runs in its own thread and calls back into teleop
    from pynput import keyboard

    quit_event = asyncio.Event()

    def handle_press(key):
        try:
            char = key.char.lower() if key.char else None
        except AttributeError:
            char = None
        if char:
            teleop.on_press(char)

    def handle_release(key):
        if key == keyboard.Key.esc:
            teleop.loop.call_soon_threadsafe(quit_event.set)
            return False  # stop listener
        try:
            char = key.char.lower() if key.char else None
        except AttributeError:
            char = None
        if char == 'q':
            teleop.loop.call_soon_threadsafe(quit_event.set)
            return False
        if char:
            teleop.on_release(char)

    listener = keyboard.Listener(on_press=handle_press, on_release=handle_release)
    listener.start()

    await quit_event.wait()
    listener.stop()
    await teleop.disconnect()


if __name__ == "__main__":
    asyncio.run(main())