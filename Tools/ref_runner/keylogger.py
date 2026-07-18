import json
import sys
import time
from pathlib import Path

import keyboard

LOG_FILE = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path("input_log.json").resolve()
STOP_FILE = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None

KEY_MAP = {
    "up": "UP",
    "num 8": "UP",

    "down": "DOWN",
    "num 2": "DOWN",

    "left": "LEFT",
    "num 4": "LEFT",

    "right": "RIGHT",
    "num 6": "RIGHT",

    "left ctrl": "FIRE",
    "right ctrl": "FIRE",
    "left shift": "FIRE",
    "right shift": "FIRE",
    "ctrl": "FIRE",
    "shift": "FIRE",

    "space": "FIRE2",
    "num enter": "FIRE2",

    "backspace": "SELECT",
    "enter": "SELECT",
}

input_log = []
pressed_at = {}
last_action_end = None
start_time = time.perf_counter()


def ms(seconds: float) -> int:
    return round(seconds * 1000)


def add_wait_if_needed(now: float) -> None:
    global last_action_end

    if last_action_end is None:
        wait_ms = ms(now - start_time)
    else:
        wait_ms = ms(now - last_action_end)

    if wait_ms > 0:
        input_log.append({"waitMs": wait_ms})


def save_json() -> None:
    LOG_FILE.parent.mkdir(parents=True, exist_ok=True)
    tmp_file = LOG_FILE.with_suffix(LOG_FILE.suffix + ".tmp")

    with tmp_file.open("w", encoding="utf-8") as f:
        json.dump({"input": input_log}, f, ensure_ascii=False, indent=2)

    tmp_file.replace(LOG_FILE)


def flush_pressed_keys() -> None:
    """Close still-held keys when the logger is stopped."""
    global last_action_end

    now = time.perf_counter()

    for action, down_time in list(pressed_at.items()):
        add_wait_if_needed(down_time)
        input_log.append({
            "press": action,
            "durationMs": max(1, ms(now - down_time)),
        })
        last_action_end = now

    pressed_at.clear()


def on_key_event(event) -> None:
    global last_action_end

    action = KEY_MAP.get(event.name)
    if action is None:
        return

    now = time.perf_counter()

    if event.event_type == "down":
        if action not in pressed_at:
            pressed_at[action] = now
        return

    if event.event_type == "up":
        down_time = pressed_at.pop(action, None)
        if down_time is None:
            return

        add_wait_if_needed(down_time)

        input_log.append({
            "press": action,
            "durationMs": max(1, ms(now - down_time)),
        })

        last_action_end = now
        save_json()


def should_stop() -> bool:
    return STOP_FILE is not None and STOP_FILE.exists()


def main() -> int:
    LOG_FILE.parent.mkdir(parents=True, exist_ok=True)

    keyboard.hook(on_key_event)

    print("Key logging started.")
    print(f"Input log: {LOG_FILE}")
    if STOP_FILE is not None:
        print(f"Stop file: {STOP_FILE}")

    try:
        while not should_stop():
            time.sleep(0.1)
    except KeyboardInterrupt:
        pass
    finally:
        flush_pressed_keys()
        save_json()
        keyboard.unhook_all()
        print("Key logging stopped.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
