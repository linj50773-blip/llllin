"""Record Tide CSV serial output while sending single-character commands.

Usage: python3 logger.py /dev/cu.usbmodemXXXX logs/session01.csv
Close Arduino Serial Monitor first. Requires: pip install pyserial
"""

import csv
import sys
import threading
from pathlib import Path

import serial

HEADER = "t_ms,state,rate,fsr1,fsr2,accA,event"


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("Usage: python3 logger.py SERIAL_PORT OUTPUT.csv")
    output = Path(sys.argv[2])
    output.parent.mkdir(parents=True, exist_ok=True)
    with serial.Serial(sys.argv[1], 115200, timeout=0.1) as port, output.open(
        "w", newline="", encoding="utf-8"
    ) as file:
        writer = csv.writer(file)
        writer.writerow(HEADER.split(","))
        stop = threading.Event()

        def read_lines() -> None:
            while not stop.is_set():
                raw = port.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").strip()
                print(line)
                if line == HEADER:
                    continue
                fields = next(csv.reader([line]))
                if len(fields) == 7 and fields[0].isdigit():
                    writer.writerow(fields)
                    file.flush()

        thread = threading.Thread(target=read_lines, daemon=True)
        thread.start()
        print("Enter one command letter, or type exit to stop.")
        try:
            while True:
                command = input().strip()
                if command.lower() == "exit":
                    break
                if command:
                    port.write(command[0].encode("ascii"))
        finally:
            stop.set()
            thread.join(timeout=0.3)


if __name__ == "__main__":
    main()
