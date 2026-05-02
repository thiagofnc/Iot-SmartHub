"""
Live viewer for the SmartHub camera firmware's test-model (inference) mode.

Reads a binary frame stream from the ESP32 over serial and displays each 96x96
frame upscaled in an OpenCV window with the FOMO centroid drawn on top.

Usage:
    python tools/live_viewer.py COM5
    python tools/live_viewer.py /dev/ttyACM0

The script connects, sends 't' over serial to put the device in inference mode
(if it isn't already), then renders frames as they arrive. Make sure no other
process is holding the port (close the PlatformIO serial monitor first).

Keys (focus the OpenCV window):
    q  quit
    s  save the current frame to hard_negative_mining/ for retraining
       (use this whenever the model makes a wrong prediction — those frames
        become hard examples for the next training run)

Wire format per frame, emitted by main.cpp::runInferenceCycle whenever the
device is in inference mode:
    "FRAME <len> <has_hand> <cx> <cy>\n"
    <len bytes of raw RGB565, little-endian as stored in the framebuffer>
    "\n"
"""

import sys
import time
from pathlib import Path

import cv2
import numpy as np
import serial

BAUD = 921600
WIDTH = 96
HEIGHT = 96
DISPLAY_SCALE = 6  # 96 -> 576 px window
EXPECTED_BYTES = WIDTH * HEIGHT * 2

# Hard-negative mining output: <repo_root>/hard_negative_mining/
SAVE_DIR = Path(__file__).resolve().parent.parent / "hard_negative_mining"


def rgb565_to_bgr(buf: bytes) -> np.ndarray:
    # Try big-endian first: ESP32 camera framebuffers come out byte-swapped
    # relative to a normal LE uint16, so the 16-bit pixel is (buf[2i]<<8)|buf[2i+1].
    arr = np.frombuffer(buf, dtype=">u2").reshape(HEIGHT, WIDTH)
    r = ((arr & 0xF800) >> 8).astype(np.uint8)
    g = ((arr & 0x07E0) >> 3).astype(np.uint8)
    b = ((arr & 0x001F) << 3).astype(np.uint8)
    # OpenCV expects BGR.
    return np.dstack((b, g, r))


MAGIC = b"FRAME "


def _scan_for_magic(ser: serial.Serial) -> bool:
    # Byte-by-byte scan for the magic header. Necessary because RGB565 payload
    # can contain 0x0A bytes that look like line breaks to readline().
    window = b""
    while True:
        b = ser.read(1)
        if not b:
            return False
        window = (window + b)[-len(MAGIC):]
        if window == MAGIC:
            return True


def read_frame(ser: serial.Serial):
    if not _scan_for_magic(ser):
        return None

    # Read header up to newline.
    header = bytearray()
    while True:
        b = ser.read(1)
        if not b:
            return None
        if b == b"\n":
            break
        header += b
        if len(header) > 64:  # something is wrong
            return None

    parts = header.decode("ascii", errors="replace").strip().split()
    # After consuming "FRAME ", what remains should be: <len> <has_hand> <cx> <cy>
    if len(parts) != 4:
        return None
    try:
        length = int(parts[0])
        has_hand = parts[1] == "1"
        cx = float(parts[2])
        cy = float(parts[3])
    except ValueError:
        return None

    if length != EXPECTED_BYTES:
        return None

    payload = ser.read(length)
    if len(payload) != length:
        return None
    ser.read(1)  # trailing newline
    return payload, has_hand, cx, cy


def draw_centroid(img: np.ndarray, cx: float, cy: float) -> None:
    # Image is already upscaled; convert from 96-space to display-space.
    px = int(cx * DISPLAY_SCALE)
    py = int(cy * DISPLAY_SCALE)
    color = (0, 255, 0)
    cv2.line(img, (px - 14, py), (px + 14, py), color, 2)
    cv2.line(img, (px, py - 14), (px, py + 14), color, 2)
    cv2.circle(img, (px, py), 6, color, -1)


def main() -> int:
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <serial_port>", file=sys.stderr)
        return 1
    port = sys.argv[1]

    ser = serial.Serial(port, BAUD, timeout=1)
    print(f"Listening on {port} @ {BAUD}. Press 'q' in the window to quit.")

    # Opening the serial port pulses DTR on Windows, which resets the ESP32-S3.
    # Wait for the firmware to boot, then send 't' to enter inference mode.
    # Once in inference mode the device streams frames automatically.
    time.sleep(2.0)
    ser.reset_input_buffer()
    ser.write(b"t")
    print("Sent 't' to enter inference mode. Waiting for frames...")

    SAVE_DIR.mkdir(parents=True, exist_ok=True)

    window = "SmartHub Live"
    cv2.namedWindow(window, cv2.WINDOW_AUTOSIZE)

    frame_count = 0
    saved_count = 0
    last_gray = None  # most recent 96x96 single-channel frame, ready to save

    while True:
        frame = read_frame(ser)

        if frame is not None:
            frame_count += 1
            if frame_count % 10 == 0:
                print(f"frames received: {frame_count}")

            payload, has_hand, cx, cy = frame
            bgr = rgb565_to_bgr(payload)
            # The sensor is in grayscale mode so R==G==B; pull one channel
            # to write a true single-channel PNG for retraining.
            last_gray = bgr[:, :, 0].copy()

            big = cv2.resize(
                bgr,
                (WIDTH * DISPLAY_SCALE, HEIGHT * DISPLAY_SCALE),
                interpolation=cv2.INTER_NEAREST,
            )
            if has_hand:
                draw_centroid(big, cx, cy)
            cv2.imshow(window, big)

        key = cv2.waitKey(1) & 0xFF
        if key == ord("q"):
            break
        if key == ord("s") and last_gray is not None:
            ts = time.strftime("%Y%m%d_%H%M%S") + f"_{int(time.time() * 1000) % 1000:03d}"
            path = SAVE_DIR / f"hard_{ts}.png"
            cv2.imwrite(str(path), last_gray)
            saved_count += 1
            print(f"[SAVE] {path.name}  (total saved: {saved_count})")

    ser.close()
    cv2.destroyAllWindows()
    return 0


if __name__ == "__main__":
    sys.exit(main())
