#!/usr/bin/env python3
"""Capture one receive attempt and retain its WAV, decoder output, and report."""
import argparse
import array
import configparser
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import wave


def configured_input():
    settings = configparser.ConfigParser(interpolation=None, strict=False)
    settings.read(Path.home() / ".config/OpenSSTV/OpenSSTV.conf")
    return settings.get("audio", "input", fallback="default").strip().strip('"') or "default"


def application():
    candidates = [Path.home() / ".local/bin/tux-sstv", Path(__file__).resolve().parents[1] / "build/tux-sstv", Path.home() / ".local/bin/opensstv", Path(__file__).resolve().parents[1] / "build/opensstv"]
    found = shutil.which("tux-sstv") or shutil.which("opensstv")
    if found:
        candidates.append(Path(found))
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return str(candidate)
    raise RuntimeError("Tux-sstv could not be found.")


def capture(device, destination, seconds):
    print(f"Receive input: {device}", flush=True)
    print("Listening now. Send one complete SSTV image during the next 45 seconds.", flush=True)
    print("This test only records audio; it does not transmit or change your audio settings.", flush=True)
    process = subprocess.Popen(
        ["arecord", "-q", "-D", device, "-t", "wav", "-f", "S16_LE", "-r", "48000", "-c", "1", "-d", str(seconds), str(destination)],
        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True,
    )
    started = time.monotonic()
    try:
        while process.poll() is None and time.monotonic() - started < seconds + 8:
            time.sleep(.25)
        if process.poll() is None:
            raise RuntimeError("The audio recorder did not stop within its time limit.")
        error = process.stderr.read().strip()
        if process.returncode:
            raise RuntimeError("The receive device could not be recorded: " + error)
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=2)
        process.stderr.close()


def inspect_audio(path):
    with wave.open(str(path)) as source:
        rate, channels, width, count = source.getframerate(), source.getnchannels(), source.getsampwidth(), source.getnframes()
        if width != 2 or channels not in (1, 2):
            raise RuntimeError("The receive check expects 16-bit mono or stereo PCM WAV.")
        samples = array.array("h", source.readframes(count))
    if sys.byteorder != "little":
        samples.byteswap()
    if not samples:
        raise RuntimeError("The recording has no audio samples.")
    peak = max(map(abs, samples))
    rms = math.sqrt(sum(float(value) * value for value in samples) / len(samples))
    return {
        "sample_rate": rate, "channels": channels, "seconds": count / rate,
        "peak": peak, "rms_dbfs": 20 * math.log10(rms / 32768) if rms else None,
        "nonzero_samples": sum(value != 0 for value in samples),
        "clipped_fraction": sum(abs(value) >= 32760 for value in samples) / len(samples),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--analyze", type=Path, help="Analyze an existing WAV without opening a sound device")
    parser.add_argument("--device", help="Receive PCM; defaults to the saved Tux-sstv input")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--wait", action="store_true", help="Keep the launcher terminal open after the test")
    args = parser.parse_args()
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-%f")
    directory = args.output_dir or Path.home() / "Downloads/Tux-sstv-RX-diagnostic" / stamp
    directory.mkdir(parents=True, exist_ok=True)
    report = {"utc": stamp, "status": "not_started", "device": args.device or configured_input(), "decoder_attempts": []}
    code = 1
    try:
        recording = args.analyze.resolve() if args.analyze else directory / "received.wav"
        report["recording"] = str(recording)
        if not args.analyze:
            capture(report["device"], recording, 45)
        report["audio"] = inspect_audio(recording)
        audio = report["audio"]
        print(f"Recorded {audio['seconds']:.2f} seconds; peak {audio['peak']}/32768; clipped {audio['clipped_fraction']:.2%}.")
        if audio["nonzero_samples"] == 0:
            report["status"] = "digital_silence"
            print("NO INPUT AUDIO: every sample is zero. Check the receive input, microphone mute, and audio routing.")
            code = 2
        else:
            binary = application()
            environment = dict(os.environ, QT_QPA_PLATFORM="offscreen")
            for name, extra in [("auto", []), ("corrected", ["--auto-afc", "--auto-clock"])]:
                picture = directory / f"decoded-{name}.png"
                result = subprocess.run([binary, "--decode", str(recording), "--output", str(picture)] + extra, capture_output=True, text=True, timeout=45, env=environment)
                report["decoder_attempts"].append({"mode": name, "returncode": result.returncode, "stdout": result.stdout.strip(), "stderr": result.stderr.strip()})
                if result.returncode == 0:
                    report.update(status="decoded", image=str(picture))
                    print("IMAGE DECODED:", result.stdout.strip())
                    print("Image:", picture)
                    code = 0
                    break
            if report["status"] != "decoded":
                report["status"] = "audio_present_decode_failed"
                print("Audio reached the input, but automatic decoding failed. The original WAV is saved for investigation.")
                print(report["decoder_attempts"][-1]["stderr"])
                code = 3
    except KeyboardInterrupt:
        report.update(status="cancelled")
        code = 130
        print("Receive check stopped.")
    except (OSError, RuntimeError, ValueError, wave.Error, subprocess.TimeoutExpired) as error:
        report.update(status="error", error=str(error))
        print("Receive check:", error)
    finally:
        (directory / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        print("\nResults saved in:", directory, flush=True)
        if args.wait and sys.stdin.isatty():
            try:
                input("Press Enter to close this window. Your recording and report are saved.")
            except (EOFError, KeyboardInterrupt):
                pass
    return code


if __name__ == "__main__":
    raise SystemExit(main())
