"""Plot the LSM6DS3TR-C serial output produced by src/main.c."""

from __future__ import annotations

import argparse
from collections import deque

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import serial
from serial.tools import list_ports


SERIES = ("ax", "ay", "az", "gx", "gy", "gz")
MICRO_UNITS_PER_UNIT = 1_000_000
PLOT_REFRESH_RATE_HZ = 25


def parse_imu_line(line: str):
    """Parse one compact sample emitted by the firmware."""
    fields = line.strip().split(",")
    if len(fields) != 9 or fields[0] != "IMU":
        return None

    try:
        sequence, timestamp_us, *raw_values = map(int, fields[1:])
    except ValueError:
        return None

    values = tuple(value / MICRO_UNITS_PER_UNIT for value in raw_values)
    return sequence, timestamp_us, values


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Plot live LSM6DS3TR-C acceleration and gyroscope data."
    )
    parser.add_argument("--port", help="Serial port, for example COM5")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument(
        "--window",
        type=float,
        default=30.0,
        help="Visible time window in seconds (default: 30)",
    )
    parser.add_argument(
        "--list-ports",
        action="store_true",
        help="List available serial ports and exit",
    )
    return parser.parse_args()


def print_available_ports() -> None:
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports found.")
        return

    for port in ports:
        print(f"{port.device}: {port.description}")


def main() -> int:
    args = parse_args()

    if args.list_ports:
        print_available_ports()
        return 0

    if not args.port:
        print("Specify --port. Use --list-ports to find the board's port.")
        return 2

    try:
        serial_port = serial.Serial(args.port, args.baud, timeout=0)
    except serial.SerialException as error:
        print(f"Unable to open {args.port}: {error}")
        return 1

    times: deque[float] = deque()
    samples = {name: deque() for name in SERIES}
    receive_buffer = bytearray()
    stream_start_us: int | None = None
    last_sequence: int | None = None
    received_samples = 0
    missed_samples = 0

    figure, (accel_axes, gyro_axes) = plt.subplots(
        2, 1, sharex=True, figsize=(10, 7)
    )
    accel_lines = [
        accel_axes.plot([], [], label=axis)[0] for axis in ("X", "Y", "Z")
    ]
    gyro_lines = [
        gyro_axes.plot([], [], label=axis)[0] for axis in ("X", "Y", "Z")
    ]

    accel_axes.set_ylabel("Acceleration (m/s²)")
    gyro_axes.set_ylabel("Angular velocity (rad/s)")
    gyro_axes.set_xlabel("Time (seconds)")
    accel_axes.grid(True, alpha=0.3)
    gyro_axes.grid(True, alpha=0.3)
    accel_axes.legend(ncols=3)
    gyro_axes.legend(ncols=3)
    title = figure.suptitle(f"LSM6DS3TR-C live data — {args.port}")

    def update(_frame: int):
        nonlocal stream_start_us, last_sequence, received_samples, missed_samples

        waiting = serial_port.in_waiting
        if waiting:
            receive_buffer.extend(serial_port.read(waiting))

        while b"\n" in receive_buffer:
            raw_line, _, remaining = receive_buffer.partition(b"\n")
            receive_buffer[:] = remaining
            line = raw_line.decode("utf-8", errors="ignore")

            parsed = parse_imu_line(line)
            if parsed is None:
                continue

            sequence, timestamp_us, values = parsed
            if stream_start_us is None:
                stream_start_us = timestamp_us

            if last_sequence is not None and sequence > last_sequence + 1:
                missed_samples += sequence - last_sequence - 1
            last_sequence = sequence
            received_samples += 1

            timestamp = (timestamp_us - stream_start_us) / MICRO_UNITS_PER_UNIT
            times.append(timestamp)
            for name, value in zip(SERIES, values):
                samples[name].append(value)

            cutoff = timestamp - args.window
            while times and times[0] < cutoff:
                times.popleft()
                for series_values in samples.values():
                    series_values.popleft()

        if not times:
            return accel_lines + gyro_lines

        for line, name in zip(accel_lines, SERIES[:3]):
            line.set_data(times, samples[name])

        for line, name in zip(gyro_lines, SERIES[3:]):
            line.set_data(times, samples[name])

        left = max(0.0, times[-1] - args.window)
        right = max(args.window, times[-1])
        gyro_axes.set_xlim(left, right)

        accel_axes.relim()
        accel_axes.autoscale_view(scalex=False, scaley=True)
        gyro_axes.relim()
        gyro_axes.autoscale_view(scalex=False, scaley=True)
        title.set_text(
            f"LSM6DS3TR-C live data — {args.port} | "
            f"received: {received_samples} | missed: {missed_samples}"
        )

        return accel_lines + gyro_lines

    animation = FuncAnimation(
        figure,
        update,
        interval=1000 / PLOT_REFRESH_RATE_HZ,
        cache_frame_data=False,
    )

    try:
        plt.tight_layout()
        plt.show()
    finally:
        animation.event_source.stop()
        serial_port.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
