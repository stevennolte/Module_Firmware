"""
PC Module Management Server
============================
A Flask web application that:
  - Discovers ESP32 modules on the local network via mDNS
  - Displays live diagnostics from each module
  - Checks this GitHub repository for the latest firmware releases
  - Downloads and pushes OTA firmware updates to modules

Supported modules (initial):
  - ESP32_AIO
  - ESP32_Row_Controller
"""

import socket
import threading
import time
import os
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
]

# How long (seconds) to cache the result of a GitHub release lookup
RELEASE_CACHE_TTL = 300

app = Flask(__name__)

# ── Helpers ───────────────────────────────────────────────────────────────

def _github_headers() -> dict:
    h = {"Accept": "application/vnd.github+json"}
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
    Returns a dict with keys: version, tag, download_url, asset_name, published_at
    or None if nothing is found.
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
                for asset in release.get("assets", []):
                    aname: str = asset.get("name", "")
                    if aname.endswith(".bin") and module_name in aname:
                        # Strip the leading "MODULE_NAME_" prefix from the tag
                        # to get just the version string.
                        ver_str = tag[len(module_name):].lstrip("_").lstrip("v")
                        candidates.append({
                            "version":      ver_str,
                            "tag":          tag,
                            "download_url": asset["browser_download_url"],
                            "asset_name":   aname,
                            "published_at": release.get("published_at", ""),
                        })
                        break
            
            # Find the release with the highest version number
            if candidates:
                try:
                    result = max(candidates, key=lambda x: pkg_version.parse(x["version"]))
                except Exception as ver_err:
                    app.logger.warning("Version parsing failed for %s: %s. Using first match.", module_name, ver_err)
                    result = candidates[0]
                    
    except Exception as e:
        app.logger.warning("GitHub release lookup failed for %s: %s", module_name, e)

    with _release_cache_lock:
        _release_cache[module_name] = (time.time(), result)

    return result


# ── Routes ────────────────────────────────────────────────────────────────

@app.route("/")
def index():
    return render_template("index.html", modules=MODULES,
                           repo_owner=REPO_OWNER, repo_name=REPO_NAME)


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


@app.route("/api/module/<name>/update", methods=["POST"])
def api_push_update(name: str):
    """
    Download the latest firmware from GitHub and push it to the module via OTA.
    The module is located via mDNS and updated through its /update HTTP endpoint.
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

    # Download firmware binary from GitHub
    try:
        fw_resp = requests.get(release["download_url"], timeout=60)
        fw_resp.raise_for_status()
    except Exception as e:
        return jsonify({"error": f"Failed to download firmware: {e}"}), 500

    # Push firmware to module via its /update endpoint
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


# ── Entry point ───────────────────────────────────────────────────────────
if __name__ == "__main__":
    host = os.environ.get("HOST", "0.0.0.0")
    port = int(os.environ.get("PORT", 5000))
    debug = os.environ.get("FLASK_DEBUG", "0") == "1"
    print(f"Starting Module Management Server on http://{host}:{port}")
    print(f"GitHub repo: {REPO_OWNER}/{REPO_NAME}")
    app.run(host=host, port=port, debug=debug)
