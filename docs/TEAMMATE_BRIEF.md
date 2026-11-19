# Teammate brief: 2026-11-20

Two tasks, ahead of the 14:30 mentor meeting today. Read `CLAUDE.md` and [PROBLEM_STATEMENT.md](PROBLEM_STATEMENT.md) first; this brief only adds what they don't cover. If you're using Claude Code, point it at this file.

**Who owns what today (to avoid merge conflicts):**
- **You:** the files listed in Task 2 and nothing else.
- **Jeevant:** `backend/utils/scoring.py`, `backend/routes/risk.py`, `frontend/`, `firmware/`, `eval/`, `training/`.

Work on a branch named `security-layer`, not `main`.

---

## Task 1: overnight GPU training run (start first, it runs unattended)

This is the "from scratch" ablation. It trains AASIST-L with no pretrained weights, using exactly the same pipeline as our fine-tuned run, to answer "why not build your own model?". Expect roughly 7–9 h on an RTX 4050 laptop GPU (an estimate; our 3050 needs ~22 min per epoch).

**1. Data.** Copy these three folders from Jeevant's laptop, keeping the same layout. You do *not* need the eval set or In-the-Wild, because scoring happens on Jeevant's laptop.
```
datasets/ASVspoof2019/LA/ASVspoof2019_LA_train/
datasets/ASVspoof2019/LA/ASVspoof2019_LA_dev/
datasets/ASVspoof2019/LA/ASVspoof2019_LA_cm_protocols/
```

**2. Setup** (PowerShell). Clone the project repo and `aasist` side by side in the same parent folder:
```powershell
git clone https://github.com/JeevantVerma/Real-Time-Fraud-Detection-Smart-Glasses.git
git clone https://github.com/clovaai/aasist.git
cd aasist
python -m venv .venv
.venv\Scripts\pip install torch --index-url https://download.pytorch.org/whl/cu126
.venv\Scripts\pip install numpy scipy soundfile
.venv\Scripts\python -c "import torch; print(torch.cuda.is_available(), torch.cuda.get_device_name(0))"
```
The last line must print `True` and the GPU name.

**3. Run.** Start it from the project repo folder, with the laptop **plugged in** (on battery the GPU throttles to ~¼ speed) and sleep disabled:
```powershell
..\aasist\.venv\Scripts\python.exe -u training/voice/finetune_aasist.py --root <path-to>/datasets/ASVspoof2019/LA --run-name aasist-l_scratch_both --init scratch --epochs 30 --lr 1e-4
```
- **Check it started:** an "epoch 0 … dev EER" line should appear within ~20 min.
- **"Paging file too small" (WinError 1455):** rerun with `--workers 4`.
- **Interrupted:** rerun the same command with `--resume`. It continues from the last finished epoch.
- **One GPU job at a time.** Don't start a second copy.
- **Don't touch the training/eval split.** Never add `--train-limit` or point the script at eval data.

**4. Hand back.** Send Jeevant the folder `aasist\runs\aasist-l_scratch_both\` (a few MB). If it hasn't finished by ~13:30, send it anyway: `log.jsonl` and `best.pth` are written as it goes.

---

## Task 2: security layer, minimal version (plan Layer 6)

Under the new problem statement, the system protects government / critical-infrastructure staff from impersonators. This adds two classic security controls on top of the detectors. Keep it small, working and tested. Login/roles and HTTPS are **not** part of today's task.

### 2a. Threat-intel lookup

**New files:**
- `backend/utils/threat_intel.py`
- `backend/data/threat_indicators.json`
- `backend/routes/threat.py`
- one `include_router` line in `backend/app.py`

**Indicator file.** A JSON list of entries `{id, type, value, severity, source, added, note}`:
- `type` is one of `phone`, `domain`, `document_sha256`.
- `severity` is `low`, `medium` or `high`.
- Seed it with **clearly fictional** entries only. For domains, use reserved names such as `*.example` or `*.test`. For phone numbers, use obviously invalid ones such as `+910000000001`. Never use real people's numbers.

**Normalisation (the part that needs care):**
- **Phone:** strip spaces, dashes and brackets; map `0XXXXXXXXXX` and bare 10-digit Indian numbers to `+91XXXXXXXXXX`.
- **URL / domain:** lowercase; strip the scheme, `www.`, the port, the path and any trailing dot.
- **Subdomains:** a listed `bad.example` must also match `login.bad.example`, but must **not** match `notbad.example`.

**`POST /threat-check`.** Takes a JSON body `{phone_numbers: [], urls: [], document_sha256: []}`, all lists optional. Returns:
```json
{"module": "threat_intel", "status": "success", "matched": true,
 "threat_score": 0.0-1.0, "matches": [{"id", "type", "severity", "note"}]}
```
- `threat_score` is the highest severity matched: high = 1.0, medium = 0.6, low = 0.3, none = 0.0.
- Keep the same response style as the other routes (`module` / `status` keys).

**Privacy:**
- Don't persist the raw numbers or URLs that were checked. Indicators describe attackers, which is fine to store.
- Phone numbers checked are personal data, so at most log the matched indicator IDs.

**Don't wire it into `scoring.py`.** Jeevant integrates it into the fused verdict. Just make the function `check(phone_numbers, urls, document_sha256) -> dict` importable.

### 2b. Tamper-evident audit log

Extend `backend/utils/session_log.py` so the verdict log can't be silently edited:
- Add columns `prev_hash` and `entry_hash`, where `entry_hash = sha256(prev_hash + canonical JSON of the row's fields)`. Canonical means sorted keys and fixed float formatting. The first row's `prev_hash` is 64 zeros.
- **Migrate the existing local DB in place:** `ALTER TABLE ... ADD COLUMN` when the columns are missing. Old rows keep NULL hashes and the chain starts at the first new row. Don't delete the DB.
- Add `verify_chain() -> {"ok": bool, "checked": n, "first_bad_id": id | None}`, and expose it at `GET /sessions/verify`. Add that route in a new file, `backend/routes/audit.py`, **not** in `risk.py`.
- Keep the existing guarantee: `log_session` never raises and never stores audio, images or documents.
- **Honest limitation, for the report:** this detects edits but doesn't stop someone with DB write access from rebuilding the whole chain. The fix is to publish the latest hash elsewhere, for example `GET /sessions/head`. That endpoint is optional.

### Tests

Put them in a new file, `backend/test_security.py`. Don't edit `test_api.py`. Cover:
- phone normalisation (several formats of the same number match one indicator);
- domain match, subdomain match, and `notbad.example` **not** matching;
- an empty request returns `matched: false` and score 0;
- chain verification: verify passes after a few `log_session` calls; edit one row directly with SQL and verify reports that row's ID. Use a temporary DB by overriding `session_log.DB_PATH` in the test. Never touch the real `session_log.db` in tests.

Also run `python backend/test_api.py` before you finish, to confirm nothing existing broke.

### Done means
- Both endpoints work against a running backend.
- `test_security.py` and `test_api.py` both pass.
- The branch is pushed, with a short note to Jeevant listing the endpoints and how to try them.

---

## Rules that apply to everything here
- **No stored biometrics, ever.** No face, voice or document images saved to disk beyond the request that uses them (see `CLAUDE.md`). Threat indicators about attackers are fine.
- **Don't change** the voice demo path (`routes/voice.py`, `utils/anti_spoofing.py`) or the fusion logic. They're being demoed today.
- **Ask Jeevant** before changing anything outside the files listed above.
