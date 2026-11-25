#!/usr/bin/env python3
"""A private pseudo-terminal TS-990S for native Hamlib integration tests.

Implements the exercised subset of Kenwood's PC command reference. This owns
no real serial device or radio. Unknown commands are rejected with ?;.
"""
import json
import os
from pathlib import Path
import select
import sys

root = Path(sys.argv[1])
master, slave = os.openpty()
print(os.ttyname(slave), flush=True)
values = {"FA": "00014230000", "FB": "00007100000", "OM0": "2", "OM1": "1", "CB": "0", "TB": "0", "PC": "100", "RF": "+0000", "RT": "0", "XT": "0"}
log = root / "cat.log"
state_file = root / "cat-state.json"
state_temp = state_file.with_suffix(".tmp")
state_temp.write_text(json.dumps(values))
os.replace(state_temp, state_file)
buffer = b""
while True:
    if not select.select([master], [], [], .1)[0]:
        continue
    try:
        buffer += os.read(master, 4096)
    except OSError:
        continue
    while b";" in buffer:
        raw, buffer = buffer.split(b";", 1)
        command = raw.decode("ascii", errors="replace")
        with log.open("a") as output:
            output.write(command + "\n")
        replies = {"ID": "ID024", "AI": "AI0", "FV": "FV1.26", "PS": "PS1", "KS": "KS020", "SB": "SB0", "RT": "RT0", "XT": "XT0", "SM0": "SM00000", "TN0": "TN001", "TN1": "TN101", "TO0": "TO00", "TO1": "TO10"}
        replies["SM1"] = "SM10000"
        replies.update({key: key + value for key, value in values.items()})
        if command in replies:
            os.write(master, (replies[command] + ";").encode())
        elif command.startswith("OM0") and len(command) == 4:
            if not (root / "ignore-mode").exists():
                values["OM" + values["CB"]] = command[3]
        elif command[:2] in ("FA", "FB") and len(command) == 13:
            values[command[:2]] = command[2:]
        elif command[:2] in ("CB", "TB") and len(command) == 3:
            values[command[:2]] = command[2]
        elif command.startswith("PC") and len(command) == 5:
            # Physical power control clamps to the TS-990S's 5–200 W range.
            values["PC"] = f"{max(5, min(200, int(command[2:]))):03d}"
        elif command[:2] in ("RU", "RD"):
            delta = int(command[2:] or "10") * (1 if command.startswith("RU") else -1)
            values["RF"] = f"{int(values['RF']) + delta:+05d}"
        elif command[:2] in ("RT", "XT") and len(command) == 3:
            values[command[:2]] = command[2]
        elif command in ("AI0", "AI2", "RX", "TX", "TX0", "TX1"):
            pass
        else:
            os.write(master, b"?;")
        state_temp.write_text(json.dumps(values))
        os.replace(state_temp, state_file)
