"""Voice anti-spoofing and liveness verification routes."""

# Standard library imports
import os
import re
import shutil
import time
from uuid import uuid4

# Third-party imports
from fastapi import APIRouter, File, UploadFile, HTTPException

# Local application imports
from utils.anti_spoofing import analyze_voice_liveness

# Create a router for voice-related endpoints
router = APIRouter()

# TEMPORARY DEBUG / T3 COLLECTION — saves a lossless copy of each recording
# (the upload as received: WAV from the firmware) for listen-back and for the
# own-device (T3) evaluation set, separate from the real request-scoped upload
# (which is still deleted in the finally block below, unchanged). This is
# intentionally NOT part of the fresh-analysis identity-check path — see
# CLAUDE.md "The one rule that must never regress": nothing here is ever read
# back by the backend. Recordings go to debug_recordings/<label>/, where
# <label> is the first line of debug_recordings/current_label.txt (e.g.
# "bonafide" or "attack_tts"), so a recording session can be labeled batch by
# batch without restarting the server. DEBUG_RECORDINGS_DIR is gitignored.
DEBUG_RECORDINGS_DIR = "debug_recordings"


def _current_label() -> str:
    try:
        with open(os.path.join(DEBUG_RECORDINGS_DIR, "current_label.txt"), encoding="utf-8") as f:
            label = f.readline().strip().lower()
    except OSError:
        return "unlabeled"
    label = re.sub(r"[^a-z0-9_-]", "", label)
    return label or "unlabeled"


def _save_debug_copy(upload_path: str) -> None:
    try:
        out_dir = os.path.join(DEBUG_RECORDINGS_DIR, _current_label())
        os.makedirs(out_dir, exist_ok=True)
        ext = os.path.splitext(upload_path)[1] or ".audio"
        out_name = f"voice_{time.strftime('%Y%m%d_%H%M%S')}_{uuid4().hex[:8]}{ext}"
        shutil.copyfile(upload_path, os.path.join(out_dir, out_name))
    except Exception as exc:
        # Debug-only convenience — never let this break the real endpoint.
        print(f"[debug] failed to save debug recording: {exc}")


@router.post("/voice-check")
async def voice_check(audio: UploadFile = File(...)):
    """Handle voice anti-spoofing and audio liveness verification requests."""
    # Validate file presence
    if not audio.filename:
        raise HTTPException(status_code=400, detail="No audio file provided")

    # Ensure uploads directory exists
    os.makedirs("uploads", exist_ok=True)

    # Create a unique filename to avoid collisions
    file_ext = os.path.splitext(audio.filename)[1] or ".audio"
    safe_name = f"voice_{uuid4().hex}{file_ext}"
    file_path = os.path.join("uploads", safe_name)

    # Save uploaded audio to disk
    try:
        with open(file_path, "wb") as buffer:
            buffer.write(await audio.read())
    except OSError as exc:
        raise HTTPException(status_code=500, detail="Failed to save audio file") from exc

    try:
        # AI Voice Anti-Spoofing & Liveness Analysis (no stored reference — analyzes the live capture itself)
        liveness_res = analyze_voice_liveness(file_path)

        _save_debug_copy(file_path)  # TEMPORARY DEBUG / T3 — see note above

        return {
            "module": "voice_anti_spoofing",
            "voice_liveness_score": liveness_res["voice_liveness_score"],
            "spoof_detected": liveness_res["is_spoof"],
            "spoof_type": liveness_res["spoof_type"],
            "spoof_type_description": liveness_res["spoof_type_description"],
            "details": liveness_res["details"],
            "status": "success",
        }
    finally:
        try:
            if os.path.exists(file_path):
                os.remove(file_path)
        except OSError:
            pass
