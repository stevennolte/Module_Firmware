"""
PC Module Management Server
============================
A Flask web application that:
  - Discovers ESP32 modules on the local network via mDNS
  - Displays live diagnostics from each module
  - Checks this GitHub repository for the latest firmware releases
  - Downloads and pushes OTA firmware updates to modules
  - Logs module diagnostic data to a local CSV file
  - Syncs logged data to a Google Sheets spreadsheet when internet is available

Supported modules (initial):
  - ESP32_AIO
  - ESP32_Row_Controller
  - ESP32_WiFi_AP
  - ESP32_GPS
  - GPS_Receiver
"""

import csv
import datetime
import json
import socket
import sys
import threading
import time
import os
import webbrowser
from pathlib import Path
from packaging import version as pkg_version

import requests
from flask import Flask, jsonify, render_template, request, Response

# ── Configuration ─────────────────────────────────────────────────────────
REPO_OWNER = os.environ.get("REPO_OWNER", "stevennolte")
REPO_NAME = os.environ.get("REPO_NAME", "Module_Firmware")
GITHUB_TOKEN = os.environ.get("GITHUB_TOKEN", "")   # Optional – avoids rate limits

# Modules managed by this server.  Each entry describes one type of module;
# the mdns_name is the hostname advertised over mDNS (resolves to <name>.local).
MODULES = [
    {"name": "ESP32_AIO",            "mdns_name": "ESP32_AIO"},
    {"name": "ESP32_Row_Controller", "mdns_name": "ESP32_Row_Controller"},
    {"name": "ESP32_WiFi_AP",        "mdns_name": "ESP32_WiFi_AP"},
    {"name": "ESP32_GPS",            "mdns_name": "esp32_gps"},
    {"name": "GPS_Receiver",         "mdns_name": "GPS_Receiver"},
]

# How long (seconds) to cache the result of a GitHub release lookup
RELEASE_CACHE_TTL = 300

# ── Data logging configuration ─────────────────────────────────────────────
_SERVER_DIR = Path(__file__).parent

# Path to the local CSV log file
DATA_LOG_FILE = Path(os.environ.get("DATA_LOG_FILE",
                                    str(_SERVER_DIR / "module_data_log.csv")))
# Seconds between each data-collection poll
DATA_LOG_INTERVAL = int(os.environ.get("DATA_LOG_INTERVAL", "60"))

# Path to a Google service-account JSON key file (leave blank to disable)
GDRIVE_CREDENTIALS_FILE = os.environ.get("GDRIVE_CREDENTIALS_FILE", "")
# ID of the Google Sheets spreadsheet to append rows into
GDRIVE_SPREADSHEET_ID = os.environ.get("GDRIVE_SPREADSHEET_ID", "")
# Name of the worksheet tab inside the spreadsheet
GDRIVE_SHEET_NAME = os.environ.get("GDRIVE_SHEET_NAME", "ModuleData")
# Seconds between automatic Google Drive sync attempts
GDRIVE_SYNC_INTERVAL = int(os.environ.get("GDRIVE_SYNC_INTERVAL", "300"))

_LOG_CSV_HEADER = ["timestamp", "module_name", "module_ip", "version", "data"]
_SYNC_STATE_FILE = _SERVER_DIR / "sync_state.json"

app = Flask(__name__)

# ── Helpers ───────────────────────────────────────────────────────────────

def _github_headers() -> dict:
    h = {
        "Accept": "application/vnd.github+json",
        "User-Agent": f"{REPO_OWNER}/{REPO_NAME} PC Server"
    }
    if GITHUB_TOKEN:
        h["Authorization"] = f"Bearer {GITHUB_TOKEN}"
    return h


def resolve_mdns(hostname: str) -> str | None:
    """
    Attempt to resolve an mDNS hostname (e.g. 'ESP32_AIO.local') to an IP.
    Returns None if the host cannot be found.
    """
    try:
        return socket.gethostbyname(hostname + ".local")
    except socket.gaierror:
        pass
    # Also try with underscores replaced by hyphens (some OS mDNS stacks
    # normalise underscores to hyphens).
    try:
        return socket.gethostbyname(hostname.replace("_", "-") + ".local")
    except socket.gaierror:
        return None


def get_module_version(ip: str) -> dict | None:
    """Query a module's /version endpoint.  Returns dict or None on failure."""
    try:
        r = requests.get(f"http://{ip}/version", timeout=3)
        if r.status_code == 200:
            return r.json()
    except Exception:
        pass
    return None


def get_module_debug_vars(ip: str) -> list | None:
    """Query a module's /getDebugVars endpoint.  Returns list or None."""
    try:
        r = requests.get(f"http://{ip}/getDebugVars", timeout=5)
        if r.status_code == 200:
            return r.json()
    except Exception:
        pass
    return None


# Simple in-process release cache  {module_name: (timestamp, release_dict | None)}
_release_cache: dict = {}
_release_cache_lock = threading.Lock()


def get_latest_release(module_name: str) -> dict | None:
    """
    Fetch the latest GitHub release whose tag starts with <module_name>.
    Returns a dict with keys: version, tag, download_url, asset_name, published_at,
    and optionally fs_download_url, fs_asset_name if a LittleFS image is present.
    Returns None if nothing is found.
    """
    with _release_cache_lock:
        cached = _release_cache.get(module_name)
        if cached and (time.time() - cached[0]) < RELEASE_CACHE_TTL:
            return cached[1]

    result = None
    candidates = []
    try:
        url = (
            f"https://api.github.com/repos/{REPO_OWNER}/{REPO_NAME}/releases"
        )
        r = requests.get(url, headers=_github_headers(), timeout=10)
        if r.status_code == 200:
            # Collect all matching releases
            for release in r.json():
                tag: str = release.get("tag_name", "")
                if not tag.startswith(module_name):
                    continue
                fw_url = None
                fw_name = None
                fs_url = None
                fs_name = None
                for asset in release.get("assets", []):
                    aname: str = asset.get("name", "")
                    if not (aname.endswith(".bin") and module_name in aname):
                        continue
                    if aname.endswith(".littlefs.bin"):
                        # Filesystem image asset
                        fs_url = asset["browser_download_url"]
                        fs_name = aname
                    else:
                        # Firmware binary asset (first non-filesystem .bin wins)
                        if fw_url is None:
                            fw_url = asset["browser_download_url"]
                            fw_name = aname
                if fw_url:
                    # Strip the leading "MODULE_NAME_" prefix from the tag
                    # to get just the version string.
                    ver_str = tag[len(module_name):].lstrip("_").lstrip("v")
                    entry = {
                        "version":      ver_str,
                        "tag":          tag,
                        "download_url": fw_url,
                        "asset_name":   fw_name,
                        "published_at": release.get("published_at", ""),
                    }
                    if fs_url:
                        entry["fs_download_url"] = fs_url
                        entry["fs_asset_name"]   = fs_name
                    candidates.append(entry)
            
            # Find the release with the highest version number
            if candidates:
                try:
                    result = max(candidates, key=lambda x: pkg_version.parse(x["version"]))
                except Exception as ver_err:
                    app.logger.warning("Version parsing failed for %s: %s. Using first match.", module_name, ver_err)
                    result = candidates[0]
        else:
            app.logger.error(
                "GitHub API request failed for %s: HTTP %d - %s",
                module_name, r.status_code, r.text[:200]
            )
                    
    except Exception as e:
        app.logger.warning("GitHub release lookup failed for %s: %s", module_name, e)

    with _release_cache_lock:
        _release_cache[module_name] = (time.time(), result)

    return result


# ── Data logging ──────────────────────────────────────────────────────────

_data_log_lock = threading.Lock()

# Runtime status dict (updated by background threads)
_log_status: dict = {
    "last_log_time": None,
    "last_sync_time": None,
    "last_sync_status": "Not attempted",
    "internet_available": False,
}


def _ensure_csv_header() -> None:
    """Create the CSV file with a header row if it does not already exist."""
    if not DATA_LOG_FILE.exists():
        DATA_LOG_FILE.parent.mkdir(parents=True, exist_ok=True)
        with open(DATA_LOG_FILE, "w", newline="") as f:
            csv.writer(f).writerow(_LOG_CSV_HEADER)


def _count_csv_rows() -> int:
    """Return the number of data rows in the CSV file (excluding the header)."""
    try:
        with open(DATA_LOG_FILE, "r", newline="") as f:
            reader = csv.reader(f)
            next(reader, None)  # skip header
            return sum(1 for _ in reader)
    except Exception:
        return 0


def log_module_data() -> int:
    """
    Poll every configured module and append one row per online module to the
    local CSV log file.  Returns the number of rows written.
    If no modules are online, writes a single timestamp row.
    """
    _ensure_csv_header()
    timestamp = datetime.datetime.now(datetime.timezone.utc).isoformat()
    rows_written = 0

    with _data_log_lock:
        with open(DATA_LOG_FILE, "a", newline="") as f:
            writer = csv.writer(f)
            for mod in MODULES:
                ip = resolve_mdns(mod["mdns_name"])
                if not ip:
                    continue
                ver_info = get_module_version(ip)
                version = ver_info.get("version", "unknown") if ver_info else "unknown"
                data = get_module_debug_vars(ip)
                if data is None:
                    continue
                writer.writerow([
                    timestamp,
                    mod["name"],
                    ip,
                    version,
                    json.dumps(data),
                ])
                rows_written += 1
            
            # If no modules were online, still log a timestamp row
            if rows_written == 0:
                writer.writerow([
                    timestamp,
                    "NO_MODULES_ONLINE",
                    "",
                    "",
                    "[]",
                ])
                rows_written = 1

    _log_status["last_log_time"] = timestamp

    return rows_written


def _data_log_loop() -> None:
    """Background thread: poll modules and write data to the local CSV."""
    while True:
        try:
            log_module_data()
        except Exception as e:
            app.logger.warning("Data log error: %s", e)
        time.sleep(DATA_LOG_INTERVAL)


# ── Google Drive (Sheets) sync ────────────────────────────────────────────

def check_internet(timeout: int = 5) -> bool:
    """Return True if an outbound HTTPS connection can be established."""
    try:
        requests.get("https://www.google.com", timeout=timeout)
        return True
    except requests.RequestException:
        return False


def _get_sync_state() -> dict:
    """Load the sync-state file; return defaults when the file is absent."""
    try:
        if _SYNC_STATE_FILE.exists():
            return json.loads(_SYNC_STATE_FILE.read_text())
    except Exception:
        pass
    return {"last_synced_row": 0}


def _save_sync_state(state: dict) -> None:
    """Persist the sync-state dict to disk."""
    try:
        _SYNC_STATE_FILE.write_text(json.dumps(state))
    except Exception as e:
        app.logger.warning("Failed to save sync state: %s", e)


def get_sheets_service():
    """
    Build an authenticated Google Sheets API service using a service-account
    key file.  Returns None when credentials are unavailable or invalid.
    """
    if not GDRIVE_CREDENTIALS_FILE or not os.path.exists(GDRIVE_CREDENTIALS_FILE):
        return None
    try:
        from google.oauth2.service_account import Credentials       # type: ignore
        from googleapiclient.discovery import build                  # type: ignore

        creds = Credentials.from_service_account_file(
            GDRIVE_CREDENTIALS_FILE,
            scopes=["https://www.googleapis.com/auth/spreadsheets"],
        )
        return build("sheets", "v4", credentials=creds, cache_discovery=False)
    except Exception as e:
        app.logger.warning("Google Sheets service creation failed: %s", e)
        return None


def _ensure_sheet_header(service, spreadsheet_id: str, sheet_name: str) -> None:
    """Append a header row to the sheet if it is currently empty."""
    try:
        result = service.spreadsheets().values().get(
            spreadsheetId=spreadsheet_id,
            range=f"{sheet_name}!A1:E1",
        ).execute()
        if not result.get("values"):
            service.spreadsheets().values().append(
                spreadsheetId=spreadsheet_id,
                range=f"{sheet_name}!A1",
                valueInputOption="RAW",
                body={"values": [["Timestamp", "Module", "IP", "Version", "Debug Variables"]]},
            ).execute()
    except Exception as e:
        app.logger.warning("Failed to ensure sheet header: %s", e)


def sync_to_gdrive() -> dict:
    """
    Append any rows that have not yet been synced to Google Sheets.

    Returns a dict with:
      success (bool), synced (int), message (str)
    """
    if not GDRIVE_SPREADSHEET_ID:
        return {"success": False, "synced": 0,
                "message": "GDRIVE_SPREADSHEET_ID not configured"}

    service = get_sheets_service()
    if not service:
        return {"success": False, "synced": 0,
                "message": "Google Sheets service unavailable – check credentials"}

    if not DATA_LOG_FILE.exists():
        return {"success": True, "synced": 0, "message": "No data to sync yet"}

    state = _get_sync_state()
    last_synced_row = state.get("last_synced_row", 0)

    try:
        with _data_log_lock:
            with open(DATA_LOG_FILE, "r", newline="") as f:
                reader = csv.reader(f)
                next(reader, None)  # skip header
                all_rows = list(reader)
    except Exception as e:
        return {"success": False, "synced": 0, "message": f"Failed to read log file: {e}"}

    rows_to_sync = all_rows[last_synced_row:]
    if not rows_to_sync:
        return {"success": True, "synced": 0, "message": "Already up to date"}

    try:
        _ensure_sheet_header(service, GDRIVE_SPREADSHEET_ID, GDRIVE_SHEET_NAME)
        result = service.spreadsheets().values().append(
            spreadsheetId=GDRIVE_SPREADSHEET_ID,
            range=f"{GDRIVE_SHEET_NAME}!A1",
            valueInputOption="RAW",
            body={"values": rows_to_sync},
        ).execute()
        app.logger.info("Google Sheets API response: %s", result)
    except Exception as e:
        app.logger.error("Google Sheets API error: %s", e, exc_info=True)
        return {"success": False, "synced": 0, "message": f"Google Sheets API error: {e}"}

    new_count = last_synced_row + len(rows_to_sync)
    _save_sync_state({"last_synced_row": new_count})

    now = datetime.datetime.now(datetime.timezone.utc).isoformat()
    _log_status["last_sync_time"] = now
    _log_status["last_sync_status"] = "success"

    return {
        "success": True,
        "synced": len(rows_to_sync),
        "message": f"Synced {len(rows_to_sync)} row(s) to Google Sheets",
    }


def _gdrive_sync_loop() -> None:
    """Background thread: periodically sync the CSV log to Google Sheets."""
    while True:
        try:
            internet = check_internet()
            _log_status["internet_available"] = internet
            if internet and GDRIVE_SPREADSHEET_ID:
                result = sync_to_gdrive()
                if not result["success"]:
                    app.logger.warning("Google Drive sync failed: %s", result["message"])
                else:
                    app.logger.info("Google Drive sync: %s", result["message"])
        except Exception as e:
            app.logger.warning("Google Drive sync error: %s", e)
        time.sleep(GDRIVE_SYNC_INTERVAL)


# ── Routes ────────────────────────────────────────────────────────────────

@app.route("/")
def index():
    return render_template("index.html", modules=MODULES,
                           repo_owner=REPO_OWNER, repo_name=REPO_NAME)


@app.route("/data")
def data_viewer():
    """Data viewer page."""
    return render_template("data.html")


@app.route("/charts")
def charts_viewer():
    """Charts viewer page."""
    return render_template("charts.html")


@app.route("/style-guide")
def style_guide():
    """UI style guide / design-standard reference page."""
    return render_template("style_guide.html")


@app.route("/api/modules")
def api_modules():
    """Return live status for all managed modules."""
    result = []
    for mod in MODULES:
        ip = resolve_mdns(mod["mdns_name"])
        entry = {
            "name":      mod["name"],
            "mdns_name": mod["mdns_name"],
            "ip":        ip,
            "online":    False,
        }
        if ip:
            ver = get_module_version(ip)
            if ver:
                entry["online"]          = True
                entry["current_version"] = ver.get("version", "unknown")
                entry["current_name"]    = ver.get("name",    mod["name"])
        result.append(entry)
    return jsonify(result)


@app.route("/api/module/<name>/diagnostics")
def api_diagnostics(name: str):
    """Return raw /getDebugVars list from the named module."""
    mod = next((m for m in MODULES if m["name"] == name), None)
    if not mod:
        return jsonify({"error": "Unknown module"}), 404

    ip = resolve_mdns(mod["mdns_name"])
    if not ip:
        return jsonify({"error": "Module not reachable on network"}), 503

    data = get_module_debug_vars(ip)
    if data is None:
        return jsonify({"error": "Failed to fetch diagnostics"}), 503
    return jsonify(data)


@app.route("/api/module/<name>/latest-firmware")
def api_latest_firmware(name: str):
    """Return the latest GitHub release info for the named module."""
    mod = next((m for m in MODULES if m["name"] == name), None)
    if not mod:
        return jsonify({"error": "Unknown module"}), 404

    release = get_latest_release(name)
    if not release:
        return jsonify({"error": "No firmware release found on GitHub"}), 404
    return jsonify(release)


@app.route("/api/module/<name>/refresh-github-version", methods=["POST"])
def api_refresh_github_version(name: str):
    """Clear the release cache and fetch fresh version info from GitHub."""
    mod = next((m for m in MODULES if m["name"] == name), None)
    if not mod:
        return jsonify({"error": "Unknown module"}), 404

    # Clear the cache for this module
    with _release_cache_lock:
        if name in _release_cache:
            del _release_cache[name]

    # Fetch fresh data from GitHub
    release = get_latest_release(name)
    if not release:
        return jsonify({"error": "No firmware release found on GitHub"}), 404
    return jsonify(release)


@app.route("/api/module/<name>/update", methods=["POST"])
def api_push_update(name: str):
    """
    Download the latest firmware (and filesystem image if available) from GitHub
    and push them to the module via OTA.
    If a filesystem image is present in the release, it is pushed first via
    /updatefs, the module reboots, and then the firmware is pushed via /update.
    """
    mod = next((m for m in MODULES if m["name"] == name), None)
    if not mod:
        return jsonify({"error": "Unknown module"}), 404

    ip = resolve_mdns(mod["mdns_name"])
    if not ip:
        return jsonify({"error": "Module not reachable on network"}), 503

    release = get_latest_release(name)
    if not release:
        return jsonify({"error": "No firmware release found on GitHub"}), 404

    # ── Step 1: Push filesystem image (if available) ──────────────────────
    if release.get("fs_download_url"):
        try:
            fs_resp = requests.get(release["fs_download_url"], timeout=60)
            fs_resp.raise_for_status()
        except Exception as e:
            return jsonify({"error": f"Failed to download filesystem image: {e}"}), 500

        try:
            fs_files = {
                "filesystem": (
                    release["fs_asset_name"],
                    fs_resp.content,
                    "application/octet-stream",
                )
            }
            fs_update_resp = requests.post(
                f"http://{ip}/updatefs", files=fs_files, timeout=120
            )
            if fs_update_resp.status_code != 200:
                return jsonify({
                    "error": f"Filesystem update failed: {fs_update_resp.text}",
                    "firmware_version": release["version"],
                    "module_ip": ip,
                }), fs_update_resp.status_code
        except Exception as e:
            return jsonify({"error": f"Failed to push filesystem to module: {e}"}), 500

        # Wait for the module to reboot and its web server to become ready.
        # mDNS registers early (during WiFi connect) but server.begin() is
        # called later in the boot sequence, so we poll /version until the
        # web server actually responds rather than relying on a fixed sleep.
        REBOOT_POLL_TIMEOUT = 60    # seconds to wait for web server readiness
        INITIAL_REBOOT_DELAY = 8    # seconds to let the reboot start
        REBOOT_POLL_INTERVAL = 3    # seconds between readiness checks

        reboot_deadline = time.time() + REBOOT_POLL_TIMEOUT
        time.sleep(INITIAL_REBOOT_DELAY)  # initial pause to let the reboot start

        ip = None
        while time.time() < reboot_deadline:
            candidate_ip = resolve_mdns(mod["mdns_name"])
            if candidate_ip and get_module_version(candidate_ip) is not None:
                ip = candidate_ip
                break
            time.sleep(REBOOT_POLL_INTERVAL)

        if not ip:
            return jsonify({"error": "Module did not come back online after filesystem update"}), 503

    # ── Step 2: Download and push firmware binary ─────────────────────────
    try:
        fw_resp = requests.get(release["download_url"], timeout=60)
        fw_resp.raise_for_status()
    except Exception as e:
        return jsonify({"error": f"Failed to download firmware: {e}"}), 500

    try:
        files = {
            "firmware": (
                release["asset_name"],
                fw_resp.content,
                "application/octet-stream",
            )
        }
        update_resp = requests.post(
            f"http://{ip}/update", files=files, timeout=120
        )
    except Exception as e:
        return jsonify({"error": f"Failed to push firmware to module: {e}"}), 500

    return jsonify({
        "status":           "ok" if update_resp.status_code == 200 else "error",
        "message":          update_resp.text,
        "firmware_version": release["version"],
        "module_ip":        ip,
        "filesystem_updated": bool(release.get("fs_download_url")),
    }), update_resp.status_code


@app.route("/api/module/<name>/reboot", methods=["POST"])
def api_reboot(name: str):
    """Send a reboot command to the named module."""
    mod = next((m for m in MODULES if m["name"] == name), None)
    if not mod:
        return jsonify({"error": "Unknown module"}), 404

    ip = resolve_mdns(mod["mdns_name"])
    if not ip:
        return jsonify({"error": "Module not reachable on network"}), 503

    try:
        r = requests.get(f"http://{ip}/reboot", timeout=5)
        return jsonify({"status": "ok", "message": r.text})
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/data-log/status")
def api_data_log_status():
    """Return current data-logging and Google Drive sync status."""
    state = _get_sync_state()
    return jsonify({
        "last_log_time":            _log_status["last_log_time"],
        "last_sync_time":           _log_status["last_sync_time"],
        "last_sync_status":         _log_status["last_sync_status"],
        "internet_available":       _log_status["internet_available"],
        "rows_logged":              _count_csv_rows(),
        "rows_synced":              state.get("last_synced_row", 0),
        "log_file":                 str(DATA_LOG_FILE),
        "log_interval_seconds":     DATA_LOG_INTERVAL,
        "gdrive_enabled":           bool(GDRIVE_SPREADSHEET_ID and GDRIVE_CREDENTIALS_FILE),
        "gdrive_sync_interval_seconds": GDRIVE_SYNC_INTERVAL,
    })


@app.route("/api/data-log/sync-gdrive", methods=["POST"])
def api_sync_gdrive():
    """Manually trigger an immediate Google Drive sync."""
    internet = check_internet()
    _log_status["internet_available"] = internet
    if not internet:
        return jsonify({"success": False, "synced": 0,
                        "message": "No internet connection available"}), 503
    result = sync_to_gdrive()
    status_code = 200 if result["success"] else 500
    return jsonify(result), status_code


@app.route("/api/data-log/data")
def api_data_log_data():
    """Return the logged data as JSON (supports pagination)."""
    limit = request.args.get("limit", default=100, type=int)
    offset = request.args.get("offset", default=0, type=int)
    
    if not DATA_LOG_FILE.exists():
        return jsonify({"data": [], "total": 0})
    
    try:
        with _data_log_lock:
            with open(DATA_LOG_FILE, "r", newline="") as f:
                reader = csv.DictReader(f)
                all_rows = list(reader)
        
        total = len(all_rows)
        # Return rows in reverse chronological order (newest first)
        paginated = list(reversed(all_rows))[offset:offset + limit]
        
        return jsonify({
            "data": paginated,
            "total": total,
            "offset": offset,
            "limit": limit
        })
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/data-log/parsed")
def api_data_log_parsed():
    """Return parsed numeric data suitable for charting."""
    limit = request.args.get("limit", default=500, type=int)
    module = request.args.get("module", default="", type=str)
    
    if not DATA_LOG_FILE.exists():
        return jsonify({"data": [], "total": 0})
    
    try:
        with _data_log_lock:
            with open(DATA_LOG_FILE, "r", newline="") as f:
                reader = csv.DictReader(f)
                all_rows = list(reader)
        
        # Filter by module if specified
        if module:
            all_rows = [r for r in all_rows if r.get("module_name") == module]
        
        # Take most recent rows
        recent_rows = all_rows[-limit:] if len(all_rows) > limit else all_rows
        
        parsed_data = []
        for row in recent_rows:
            if row.get("module_name") == "NO_MODULES_ONLINE":
                continue
                
            parsed = {
                "timestamp": row.get("timestamp"),
                "module_name": row.get("module_name"),
                "module_ip": row.get("module_ip"),
                "version": row.get("version"),
                "metrics": {}
            }
            
            # Parse the debug data string
            try:
                data_array = json.loads(row.get("data", "[]"))
                if isinstance(data_array, list):
                    data_str = " ".join(data_array)
                    parsed["metrics"] = _parse_debug_metrics(data_str)
            except Exception:
                pass
            
            parsed_data.append(parsed)
        
        return jsonify({
            "data": parsed_data,
            "total": len(parsed_data)
        })
    except Exception as e:
        return jsonify({"error": str(e)}), 500


def _parse_debug_metrics(data_str: str) -> dict:
    """Parse numeric metrics from debug data string."""
    metrics = {}
    
    # Define patterns to extract (key phrases and their extraction)
    patterns = {
        "timestamp_boot": r"Timestamp since boot \[s\]: ([\d.]+)",
        "free_heap": r"Free Heap[: ]+(\d+)",
        "min_free_heap": r"Min free heap[: ]+(\d+)",
        "cpu_freq": r"CPU frequency[: ]+(\d+)",
        "satellites": r"Satellites[: ]+(\d+)",
        "latitude": r"Latitude[: ]+([\d.]+)",
        "longitude": r"Longitude[: ]+([\d.]+)",
        "altitude": r"Altitude[: ]+([\d.]+)",
        "speed": r"Speed[: ]+([\d.]+)",
        "heading": r"Heading[: ]+([\d.]+)",
        "hdop": r"HDOP[: ]+([\d.]+)",
        "target_angle": r"Target Angle[: ]+([-\d.]+)",
        "actual_angle": r"Actual Angle[: ]+([-\d.]+)",
        "pwm_command": r"PWM Command[: ]+(\d+)",
        "steer_loop_time": r"Steer Loop Time[: ]+(\d+)ms",
        "i2c_cycle_time": r"I2C Task Cycle Time[: ]+(\d+)ms",
        "ntrip_packets": r"Packets Received[: ]+(\d+)",
        "ntrip_bytes": r"Total Bytes[: ]+(\d+)",
        "ntrip_frequency": r"Packet Frequency[: ]+([\d.]+)",
        "panda_frequency": r"PANDA Frequency[: ]+([\d.]+)",
        "imu_heading": r"IMU Heading[: ]+([-\d]+)",
        "imu_pitch": r"IMU Pitch[: ]+([-\d]+)",
        "imu_roll": r"IMU Roll[: ]+([-\d]+)",
    }
    
    import re
    for key, pattern in patterns.items():
        match = re.search(pattern, data_str, re.IGNORECASE)
        if match:
            try:
                metrics[key] = float(match.group(1))
            except ValueError:
                pass
    
    return metrics


@app.route("/api/server/stop", methods=["POST"])
def api_server_stop():
    """Gracefully shut down the server."""
    import signal

    def _shutdown():
        time.sleep(0.5)  # Give the response time to be sent
        os.kill(os.getpid(), signal.SIGTERM)

    threading.Thread(target=_shutdown, daemon=True).start()
    return jsonify({"status": "ok", "message": "Server shutting down…"})


def _is_port_in_use(port: int, host: str = "127.0.0.1") -> bool:
    """Return True if the given TCP port is already bound."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(1)
        return s.connect_ex((host, port)) == 0


# ── Entry point ───────────────────────────────────────────────────────────
if __name__ == "__main__":
    host = os.environ.get("HOST", "0.0.0.0")
    port = int(os.environ.get("PORT", 5000))
    debug = os.environ.get("FLASK_DEBUG", "0") == "1"

    # Verify that no other instance is already running on this port.
    # Skip this check in the Werkzeug reloader child process (WERKZEUG_RUN_MAIN=true),
    # since the parent already holds the port in debug/reload mode.
    if not (debug and os.environ.get("WERKZEUG_RUN_MAIN") == "true"):
        # Always connect via 127.0.0.1: a server bound to 0.0.0.0 is reachable
        # there, and 0.0.0.0 itself is not a valid connect destination.
        if _is_port_in_use(port):
            print(
                f"ERROR: Port {port} is already in use. "
                "Another instance of the server may already be running."
            )
            sys.exit(1)

    # Start background threads.  When Flask's reloader is active it forks a
    # child process; we only want threads in the child (WERKZEUG_RUN_MAIN=true)
    # or when the reloader is disabled (non-debug mode).
    if not debug or os.environ.get("WERKZEUG_RUN_MAIN") == "true":
        # Check internet connectivity at startup
        _log_status["internet_available"] = check_internet()
        
        threading.Thread(target=_data_log_loop, daemon=True,
                         name="data-log").start()
        threading.Thread(target=_gdrive_sync_loop, daemon=True,
                         name="gdrive-sync").start()

    print(f"Starting Module Management Server on http://{host}:{port}")
    print(f"GitHub repo: {REPO_OWNER}/{REPO_NAME}")
    print(f"Data logging: {DATA_LOG_FILE}  (every {DATA_LOG_INTERVAL}s)")
    if GDRIVE_SPREADSHEET_ID:
        print(f"Google Drive sync enabled – spreadsheet: {GDRIVE_SPREADSHEET_ID}")
        print(f"Internet connectivity: {'Available' if _log_status['internet_available'] else 'Not detected'}")
    else:
        print("Google Drive sync disabled (GDRIVE_SPREADSHEET_ID not set)")

    # Open the web interface in the default browser after a short delay to allow
    # the server to finish starting up.
    url = f"http://localhost:{port}"
    def _open_browser():
        try:
            webbrowser.open(url)
        except Exception:
            pass  # Non-critical: best-effort browser launch
    threading.Timer(1.5, _open_browser).start()

    app.run(host=host, port=port, debug=debug)
