import requests
import sys
from pathlib import Path
from dotenv import load_dotenv
load_dotenv("telegramCredentials.env")
import os
BASE_DIR = Path(__file__).resolve().parent
CLIPS_DIR = BASE_DIR / "clips"

BOT_TOKEN = os.getenv("BOT_TOKEN", "")
CHAT_ID = os.getenv("CHAT_ID", "")

def send(session_id: str) -> int:
    if BOT_TOKEN.startswith("PASTE_") or CHAT_ID.startswith("PASTE_"):
        print("Telegram not configured. Put BOT_TOKEN and CHAT_ID in telegramSend.py")
        return 0

    video_path = CLIPS_DIR / session_id / "output.mp4"
    if not video_path.exists():
        print(f"Video not found: {video_path}")
        return 1

    url = f"https://api.telegram.org/bot{BOT_TOKEN}/sendVideo"
    with open(video_path, "rb") as f:
        files = {"video": f}
        data = {"chat_id": CHAT_ID, "caption": f"New video (session {session_id})"}
        r = requests.post(url, data=data, files=files, timeout=120)

    if not r.ok:
        print("Telegram send failed:", r.status_code, r.text[:300])
        return 2

    print("Telegram sent OK")
    return 0

def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: python3 telegramSend.py <session_id>")
        return 10
    return send(sys.argv[1])

if __name__ == "__main__":
    raise SystemExit(main())