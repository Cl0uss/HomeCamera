import re
import subprocess
import sys
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent
FRAMES_DIR = BASE_DIR / "frames"
CLIPS_DIR = BASE_DIR / "clips"
FPS = 25

def build(session_id: str) -> int:
    session_frames = FRAMES_DIR / session_id
    session_clips = CLIPS_DIR / session_id
    session_clips.mkdir(parents=True, exist_ok=True)
    out_mp4 = session_clips / "output.mp4"

    if not session_frames.exists():
        print(f"Session frames dir not found: {session_frames}")
        return 1

    rx = re.compile(r"^\d{6}\.jpe?g$", re.IGNORECASE)
    frames = sorted([p for p in session_frames.iterdir() if p.is_file() and rx.match(p.name)])
    if not frames:
        print(f"No frames in {session_frames}")
        return 2

    i = 1
    while True:
        jpg = session_frames / f"{i:06d}.jpg"
        jpeg = session_frames / f"{i:06d}.jpeg"
        if not jpg.exists() and not jpeg.exists():
            break
        i += 1
    count = i - 1
    if count <= 0:
        print("No sequential frames starting from 000001.jpg")
        return 3

    pattern = str(session_frames / "%06d.jpg")

    cmd = [
        "ffmpeg", "-y",
        "-framerate", str(FPS),
        "-i", pattern,
        "-frames:v", str(count),
        "-c:v", "libx264",
        "-pix_fmt", "yuv420p",
        str(out_mp4)
    ]

    print("Running:", " ".join(cmd))
    p = subprocess.run(cmd, cwd=str(BASE_DIR))
    if p.returncode != 0:
        print("ffmpeg failed")
        return p.returncode

    print(f"Video created: {out_mp4}")
    return 0

def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: python3 videoBuilder.py <session_id>")
        return 10
    return build(sys.argv[1])

if __name__ == "__main__":
    raise SystemExit(main())
