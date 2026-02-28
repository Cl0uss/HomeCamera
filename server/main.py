from flask import Flask, request, jsonify
import os
import subprocess
import sys
from pathlib import Path
from threading import Lock

BASE_DIR = Path(__file__).resolve().parent
FRAMES_DIR = BASE_DIR / "frames"
CLIPS_DIR = BASE_DIR / "clips"
FRAMES_DIR.mkdir(parents=True, exist_ok=True)
CLIPS_DIR.mkdir(parents=True, exist_ok=True)

app = Flask(__name__)

_session_lock = Lock()
_current_session_id: int | None = None

def _list_numeric_dirs(p: Path) -> list[int]:
    ids: list[int] = []
    for x in p.iterdir():
        if x.is_dir() and x.name.isdigit():
            ids.append(int(x.name))
    ids.sort()
    return ids

def _next_session_id() -> int:
    # максимум по frames и clips, чтобы ID никогда не повторялся
    frames_ids = _list_numeric_dirs(FRAMES_DIR)
    clips_ids = _list_numeric_dirs(CLIPS_DIR)
    mx = 0
    if frames_ids:
        mx = max(mx, frames_ids[-1])
    if clips_ids:
        mx = max(mx, clips_ids[-1])
    return mx + 1

def _start_new_session() -> int:
    global _current_session_id
    with _session_lock:
        _current_session_id = _next_session_id()
        (FRAMES_DIR / str(_current_session_id)).mkdir(parents=True, exist_ok=True)
        return _current_session_id

def _ensure_session() -> int:
    global _current_session_id
    with _session_lock:
        if _current_session_id is None:
            _current_session_id = _next_session_id()
            (FRAMES_DIR / str(_current_session_id)).mkdir(parents=True, exist_ok=True)
        return _current_session_id

def _get_current_session() -> int | None:
    with _session_lock:
        return _current_session_id

def _end_current_session():
    global _current_session_id
    with _session_lock:
        _current_session_id = None

def _is_first_frame(filename: str) -> bool:
    name = filename.lower()
    return name.endswith("01.jpg") or name.endswith("01.jpeg")

@app.get("/")
def home():
    return jsonify({"ok": True, "current_session": _get_current_session()}), 200

@app.post("/upload/<path:name>")
def upload_raw(name):
    filename = os.path.basename(name)

    data = request.get_data(cache=False)
    if not data:
        return jsonify({"ok": False, "error": "empty body"}), 400

    if _is_first_frame(filename):
        sid = _start_new_session()
    else:
        sid = _ensure_session()

    session_frames = FRAMES_DIR / str(sid)
    save_path = session_frames / filename

    with open(save_path, "wb") as f:
        f.write(data)

    return jsonify({"ok": True, "session": sid, "saved": filename, "bytes": len(data)}), 200

@app.post("/finalize")
def finalize():
    sid = _get_current_session()
    if sid is None:
        return jsonify({"ok": False, "error": "no active session"}), 400

    vb = str(BASE_DIR / "videoBuilder.py")
    res1 = subprocess.run([sys.executable, vb, str(sid)], cwd=str(BASE_DIR))

    ts = str(BASE_DIR / "telegramSend.py")
    res2 = subprocess.run([sys.executable, ts, str(sid)], cwd=str(BASE_DIR))

    ok = (res1.returncode == 0) and (res2.returncode == 0)

    _end_current_session()

    return jsonify({
        "ok": ok,
        "session": sid,
        "videoBuilder_rc": res1.returncode,
        "telegramSend_rc": res2.returncode
    }), 200

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)
