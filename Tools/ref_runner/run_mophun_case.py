import argparse
import json
import shutil
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
KEYLOGGER = SCRIPT_DIR / "keylogger.py"
AHK_LOGGER = SCRIPT_DIR / "Mophun_Logger.ahk"
LOG_DIR = SCRIPT_DIR / "logs"


def check_file(path: Path, name: str) -> None:
    if not path.exists():
        raise FileNotFoundError(f"{name} not found: {path}")


def find_executable(exe: str) -> str:
    p = Path(exe)
    if p.exists():
        return str(p)

    resolved = shutil.which(exe)
    if resolved:
        return resolved

    return exe


def terminate_process(proc: subprocess.Popen, name: str, timeout_sec: float = 3.0) -> None:
    if proc is None or proc.poll() is not None:
        return

    proc.terminate()
    try:
        proc.wait(timeout=timeout_sec)
    except subprocess.TimeoutExpired:
        print(f"{name}: terminate timeout, killing...")
        proc.kill()
        proc.wait(timeout=timeout_sec)


def stop_ffmpeg(proc: subprocess.Popen, timeout_sec: float = 5.0) -> None:
    if proc is None or proc.poll() is not None:
        return

    try:
        if proc.stdin:
            proc.stdin.write("q\n")
            proc.stdin.flush()
        proc.wait(timeout=timeout_sec)
    except Exception:
        terminate_process(proc, "ffmpeg", timeout_sec=timeout_sec)


def build_ffmpeg_args(args, video_file: Path, window_title: str):
    ffmpeg_exe = find_executable(args.ffmpeg_exe)

    ffmpeg_args = [
        ffmpeg_exe,
        "-y",
        "-hide_banner",
        "-loglevel", "warning",
    ]

    # gdigrab and DirectShow normally expose unrelated timestamp origins.
    # Wall-clock timestamps plus explicit PTS reset give both streams
    # the same zero point.
    ffmpeg_args += [
        "-thread_queue_size", "512",
        "-use_wallclock_as_timestamps", "1",
        "-f", "gdigrab",
        "-framerate", str(args.framerate),
    ]

    if args.capture_desktop:
        if args.offset_x is not None:
            ffmpeg_args += ["-offset_x", str(args.offset_x)]
        if args.offset_y is not None:
            ffmpeg_args += ["-offset_y", str(args.offset_y)]
        if args.video_size:
            ffmpeg_args += ["-video_size", args.video_size]
        ffmpeg_args += ["-i", "desktop"]
    else:
        ffmpeg_args += ["-i", f"title={window_title}"]

    if args.audio_device:
        ffmpeg_args += [
            "-thread_queue_size", "4096",
            "-rtbufsize", "128M",
            "-f", "dshow",
            "-audio_buffer_size", "100",
            "-i", f"audio={args.audio_device}",
        ]

    ffmpeg_args += [
        "-map", "0:v:0",
    ]

    if args.audio_device:
        ffmpeg_args += ["-map", "1:a:0"]

    ffmpeg_args += [
        "-vf", "setpts=PTS-STARTPTS",
        "-c:v", "libx264",
        "-preset", "veryfast",
        "-crf", "23",
        "-pix_fmt", "yuv420p",
    ]

    if args.audio_device:
        sync_ms = int(args.audio_sync_ms)

        if sync_ms > 0:
            # Positive value: delay audio.
            audio_filter = (
                f"asetpts=PTS-STARTPTS,aresample=async=1000:first_pts=0"
                f"adelay={sync_ms}:all=1,"
                f"aresample=async=1:first_pts=0"
            )
        elif sync_ms < 0:
            # Negative value: advance audio by trimming its beginning.
            trim_sec = abs(sync_ms) / 1000.0
            audio_filter = (
                f"atrim=start={trim_sec:.6f},"
                f"asetpts=PTS-STARTPTS,"
                f"aresample=async=1:first_pts=0"
            )
        else:
            audio_filter = (
                "asetpts=PTS-STARTPTS,"
                "aresample=async=1:first_pts=0"
            )

        ffmpeg_args += [
            "-af", audio_filter,
            "-c:a", "aac",
            "-b:a", "160k",
            "-ar", "44100",
            "-ac", "2",
        ]
    else:
        ffmpeg_args += ["-an"]

    ffmpeg_args += [
        "-movflags", "+faststart",
        str(video_file),
    ]
    return ffmpeg_args


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run one Mophun game session and capture trace log, input log and optional video."
    )
    parser.add_argument("mpn", help="Path to .mpn game file")

    parser.add_argument("--mophun-exe", required=True,
                        help="Path to mophun.exe")
    parser.add_argument("--autohotkey-exe", required=True,
                        help="Path to AutoHotkey.exe v1")
    parser.add_argument("--ffmpeg-exe", required=True,
                        help='Path to ffmpeg.exe, or "ffmpeg" if it is in PATH')

    parser.add_argument("--duration", type=float, default=30.0,
                        help="Session duration in seconds. Default: 30")
    parser.add_argument("--device-name", default=None,
                        help="Device/session label, e.g. T310 or T610")
    parser.add_argument("--width", type=int, required=True,
                        help="Mophun screen width")
    parser.add_argument("--height", type=int, required=True,
                        help="Mophun screen height")
    parser.add_argument("--zoom", type=int, default=2,
                        help="Mophun zoom. Default: 2")
    parser.add_argument("--profile", required=True,
                        help="Mophun profile without .dll extension, e.g. model1")
    parser.add_argument("--date", default="2003-10-31",
                        help="Mophun environment date. Default: 2003-10-31")
    parser.add_argument("--no-timestamp", action="store_true",
                        help="Do not add timestamp to session folder name")

    parser.add_argument("--record-video", action="store_true",
                        help="Record video with ffmpeg")
    parser.add_argument("--audio-device", default=None,
                        help='FFmpeg dshow audio device name, e.g. "Stereo Mix (2- Realtek(R) Audio)"')
    parser.add_argument("--audio-sync-ms", type=int, default=0,
                        help="Manual audio sync correction in milliseconds: negative = earlier, positive = later")
    parser.add_argument("--framerate", type=int, default=30,
                        help="Video framerate. Default: 30")
    parser.add_argument("--capture-desktop", action="store_true",
                        help="Capture a fixed desktop region instead of window title")
    parser.add_argument("--offset-x", type=int, default=None,
                        help="Desktop capture X offset, used with --capture-desktop")
    parser.add_argument("--offset-y", type=int, default=None,
                        help="Desktop capture Y offset, used with --capture-desktop")
    parser.add_argument("--video-size", default=None,
                        help='Desktop capture size, e.g. "220x220", used with --capture-desktop')
    parser.add_argument("--window-title", default=None,
                        help='Window title for ffmpeg gdigrab. Default: "<filename.mpn> - mophun"')
    parser.add_argument("--video-delay", type=float, default=1.5,
                        help="Seconds to wait after starting emulator before starting ffmpeg. Default: 1.5")

    args = parser.parse_args()

    game_file = Path(args.mpn).resolve()
    mophun_exe = Path(args.mophun_exe).resolve()
    autohotkey_exe = Path(args.autohotkey_exe).resolve()

    check_file(game_file, "MPN file")
    check_file(mophun_exe, "mophun.exe")
    check_file(autohotkey_exe, "AutoHotkey.exe")
    check_file(KEYLOGGER, "keylogger.py")
    check_file(AHK_LOGGER, "Mophun_Logger.ahk")

    if args.record_video:
        if args.ffmpeg_exe.lower() != "ffmpeg":
            check_file(Path(args.ffmpeg_exe), "ffmpeg.exe")
        elif shutil.which(args.ffmpeg_exe) is None:
            raise FileNotFoundError("ffmpeg.exe not found in PATH. Pass --ffmpeg-exe C:\\path\\to\\ffmpeg.exe")

    mpn_name = game_file.stem
    suffix = "" if args.no_timestamp else "_" + datetime.now().strftime("%Y%m%d_%H%M%S")

    device_prefix = f"{args.device_name}_" if args.device_name else ""
    session_dir = LOG_DIR / f"{device_prefix}{mpn_name}{suffix}"
    session_dir.mkdir(parents=True, exist_ok=True)

    trace_log = session_dir / "trace.log"
    input_log = session_dir / "input.json"
    video_file = session_dir / "video.mp4"
    metadata_file = session_dir / "session.json"

    stop_keylogger = session_dir / "stop_keylogger.flag"
    stop_trace = session_dir / "stop_trace.flag"

    for flag in (stop_keylogger, stop_trace):
        if flag.exists():
            flag.unlink()

    mophun_args = [
        str(mophun_exe),
        f"-d{args.date}",
        f"-w{args.width}",
        f"-h{args.height}",
        f"-z{args.zoom}",
        f"-t{args.profile}",
        "-r",
        str(game_file),
    ]

    window_title = args.window_title or f"{game_file.name} - mophun"

    metadata = {
        "mpn": str(game_file),
        "mpnName": mpn_name,
        "deviceName": args.device_name,
        "durationSec": args.duration,
        "mophunExe": str(mophun_exe),
        "mophunArgs": mophun_args,
        "width": args.width,
        "height": args.height,
        "zoom": args.zoom,
        "profile": args.profile,
        "date": args.date,
        "traceLog": str(trace_log),
        "inputLog": str(input_log),
        "video": str(video_file) if args.record_video else None,
        "recordVideo": args.record_video,
        "audioDevice": args.audio_device,
        "audioSyncMs": args.audio_sync_ms,
        "windowTitle": window_title,
        "createdAt": datetime.now().isoformat(timespec="seconds"),
    }
    metadata_file.write_text(json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8")

    keylogger_proc = None
    ahk_proc = None
    mophun_proc = None
    ffmpeg_proc = None

    try:
        print("Starting key logger...")
        keylogger_proc = subprocess.Popen([
            sys.executable,
            str(KEYLOGGER),
            str(input_log),
            str(stop_keylogger),
        ], cwd=str(SCRIPT_DIR))

        print("Starting Mophun trace logger...")
        ahk_proc = subprocess.Popen([
            str(autohotkey_exe),
            str(AHK_LOGGER),
            str(trace_log),
            str(stop_trace),
        ], cwd=str(SCRIPT_DIR))

        time.sleep(0.5)

        print("Starting Mophun emulator...")
        print(" ".join(mophun_args))
        mophun_proc = subprocess.Popen(
            mophun_args,
            cwd=str(game_file.parent),
        )

        time.sleep(args.video_delay)

        if args.record_video:
            print("Starting ffmpeg video capture...")
            ffmpeg_args = build_ffmpeg_args(args, video_file, window_title)
            print(" ".join(ffmpeg_args))
            ffmpeg_proc = subprocess.Popen(
                ffmpeg_args,
                cwd=str(session_dir),
                stdin=subprocess.PIPE,
                text=True,
            )

        deadline = time.monotonic() + args.duration
        while time.monotonic() < deadline:
            if mophun_proc.poll() is not None:
                print(f"Mophun exited early with code {mophun_proc.returncode}")
                break
            time.sleep(0.2)

        if mophun_proc.poll() is None:
            if ffmpeg_proc is not None and ffmpeg_proc.poll() is None:
                print("Session duration reached, finalizing ffmpeg...")
                stop_ffmpeg(ffmpeg_proc)
            print("Closing Mophun...")
            terminate_process(mophun_proc, "mophun.exe", timeout_sec=3.0)

    finally:
        print("Stopping ffmpeg...")
        stop_ffmpeg(ffmpeg_proc)

        print("Stopping loggers...")
        try:
            stop_keylogger.write_text("stop", encoding="utf-8")
        except Exception:
            pass
        try:
            stop_trace.write_text("stop", encoding="utf-8")
        except Exception:
            pass

        time.sleep(1.0)

        terminate_process(keylogger_proc, "keylogger.py")
        terminate_process(ahk_proc, "Mophun_Logger.ahk")

        for flag in (stop_keylogger, stop_trace):
            try:
                flag.unlink()
            except FileNotFoundError:
                pass

    print("Done.")
    print(f"Session dir: {session_dir}")
    print(f"Trace log:   {trace_log}")
    print(f"Input log:   {input_log}")
    if args.record_video:
        if video_file.exists() and video_file.stat().st_size > 0:
            print(f"Video:       {video_file}")
        else:
            print(f"Video:       NOT CREATED OR EMPTY: {video_file}")
    print(f"Metadata:    {metadata_file}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
