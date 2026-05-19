# -*- coding: utf-8 -*-
"""
╔═══════════════════════════════════════════════════════════════════╗
║  TilVista  –  Tilfeldig (nor.: zufällig) + Vista (span.: Blick)  ║
║  Sub-tools:  AleaVue  (lat. alea=Würfel + fr. vue=Blick)         ║
║                  → zufällige Bild-Slideshow                       ║
║              SattumaPic  (fin. sattuma=Zufall + Pic)              ║
║                  → zufälliger Datei-Öffner mit Vorschau           ║
╠═══════════════════════════════════════════════════════════════════╣
║  @author:  Ludwig, Armin                                          ║
║  @year:    2024-2025                                              ║
║  version:  v0.4.0                                                 ║
╚═══════════════════════════════════════════════════════════════════╝

dependencies:
    pip install PyQt6
    ffmpeg in PATH – optional, enables video frame-cycling fallback

DB storage:  optegnelser/Kaivo/
    tilvista_dirs.json          ← index, one entry per directory
    <name>_img.dshow            ← image paths, one per line  (AleaVue)
    <name>_all.catalogue        ← all file paths, one per line (SattumaPic)

    Kaivo = Finnish for 'Brunnen / Quelle' → portable directory store
    .dshow    = "directory show"  (custom extension, plain text)
    .catalogue = (custom extension, plain text)

JSON schema (one entry):
    {
      "name":        "Urlaub2024_250508",
      "path":        "../../relative/dir",          # relative when possible
      "image_files": "Urlaub2024_250508_img.dshow", # filename inside Kaivo/
      "all_files":   "Urlaub2024_250508_all.catalogue",
      "scanned_at":  "2025-05-08T14:23:01"
    }

history:
    v0.4.0 – 2025 – file lists moved out of JSON into separate
                    <name>_img.dshow / <name>_all.catalogue files;
                    JSON now stores only the filenames as references.
                    CatalogueWriteWorker + CatalogueLoadWorker (both threaded).
                    Delete entry also removes associated catalogue files.
    v0.3.0 – 2025 – JSON caches image_files + all_files; auto DB-name;
                    video preview; DataKaivo → Kaivo
    v0.2.0 – 2025 – AleaVue name; Load from Directory button; scalable preview; progress bars
    v0.1.0 – 2025 – merged RanChFO + RanSlidSHW
"""

import ctypes
import json
import os
import random
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path

from PyQt6.QtCore import (
    QFileInfo, QObject, QSize, QThread, Qt, QTimer, pyqtSignal, QUrl,
)
from PyQt6.QtGui import QImageReader, QPixmap
from PyQt6.QtWidgets import (
    QAbstractItemView, QApplication, QCheckBox, QFileDialog,
    QFileIconProvider, QHBoxLayout, QLabel, QLineEdit, QListWidget,
    QMainWindow, QMessageBox, QProgressBar, QPushButton, QSizePolicy,
    QStackedWidget, QTabWidget, QVBoxLayout, QWidget,
)

# ── Optional: Qt Multimedia (video playback) ──────────────────────────────────
try:
    from PyQt6.QtMultimedia import QMediaPlayer, QAudioOutput
    from PyQt6.QtMultimediaWidgets import QVideoWidget
    HAS_MULTIMEDIA = True
except ImportError:
    HAS_MULTIMEDIA = False

# ── Optional: ffmpeg (frame-cycling fallback) ─────────────────────────────────
HAS_FFMPEG = shutil.which("ffmpeg") is not None

# ─── Windows: sleep prevention ────────────────────────────────────────────────
ES_CONTINUOUS      = 0x80000000
ES_SYSTEM_REQUIRED = 0x00000001


def _prevent_sleep():
    try:
        ctypes.windll.kernel32.SetThreadExecutionState(
            ES_CONTINUOUS | ES_SYSTEM_REQUIRED)
    except AttributeError:
        pass


def _restore_sleep():
    try:
        ctypes.windll.kernel32.SetThreadExecutionState(ES_CONTINUOUS)
    except AttributeError:
        pass


# ─── Cross-platform open ──────────────────────────────────────────────────────
def open_path(path: str):
    try:
        os.startfile(path)
    except AttributeError:
        if sys.platform == "darwin":
            subprocess.Popen(["open", path])
        else:
            subprocess.Popen(["xdg-open", path])


# ─── Paths ────────────────────────────────────────────────────────────────────
def _script_dir() -> str:
    return os.path.dirname(os.path.abspath(sys.argv[0]))


def _kaivo_dir() -> str:
    """optegnelser/Kaivo/  – directory that holds all Kaivo files."""
    return os.path.join(_script_dir(), "optegnelser", "Kaivo")


def _kaivo_db_path() -> str:
    """optegnelser/Kaivo/tilvista_dirs.json"""
    return os.path.join(_kaivo_dir(), "tilvista_dirs.json")


def _safe_filename(name: str) -> str:
    """Strip characters that are invalid in filenames (cross-platform)."""
    for c in r'\/:*?"<>|':
        name = name.replace(c, "_")
    return name.strip(".")  # leading dots are hidden on Unix


def _log_dir() -> str:
    return os.path.join(_script_dir(), "optegnelser")


# ─── Auto DB-entry name ───────────────────────────────────────────────────────
def auto_entry_name(path: str) -> str:
    """'<lowest_folder>_<yymmdd>'  e.g.  'Fotos_250508'"""
    folder = os.path.basename(os.path.normpath(path)) or path
    return f"{folder}_{datetime.now().strftime('%y%m%d')}"


# ─── Threading utility ────────────────────────────────────────────────────────
def start_threaded_worker(worker: QObject, result_slot) -> tuple:
    thread = QThread()
    worker.moveToThread(thread)
    thread.started.connect(worker.run)
    worker.signal_result.connect(result_slot)
    thread.start()
    return thread, worker


def cleanup_thread(thread: QThread, worker: QObject):
    thread.quit()
    worker.deleteLater()
    thread.deleteLater()


# ════════════════════════════════════════════════════════════════════════════
#  FILE-TYPE SETS
# ════════════════════════════════════════════════════════════════════════════

IMAGE_SUFFIXES = {
    ".jpg", ".jpeg", ".png", ".bmp",
    ".tif", ".tiff", ".gif", ".webp",
}
VIDEO_SUFFIXES = {
    ".mp4", ".mkv", ".avi", ".mov", ".wmv",
    ".flv", ".webm", ".m4v", ".ts", ".mts",
}


# ════════════════════════════════════════════════════════════════════════════
#  WORKERS
# ════════════════════════════════════════════════════════════════════════════

class ScanWorker(QObject):
    """
    Scans a directory recursively.
    Emits both image_files and all_files lists for caching in the DB.
    """
    signal_result   = pyqtSignal(tuple)  # (ok, image_files, all_files)
    signal_progress = pyqtSignal(int)    # 0-100

    def __init__(self, directory: str):
        super().__init__()
        self.directory = directory

    def run(self):
        image_files = []
        all_files   = []
        try:
            # Collect everything first for accurate progress
            collected = []
            for root, _, files in os.walk(self.directory):
                for f in files:
                    collected.append(os.path.join(root, f))
            total = len(collected) or 1
            for i, fp in enumerate(collected):
                all_files.append(fp)
                if Path(fp).suffix.lower() in IMAGE_SUFFIXES:
                    image_files.append(fp)
                self.signal_progress.emit(int((i + 1) / total * 100))
            self.signal_result.emit((True, image_files, all_files))
        except Exception as e:
            print(f"[ScanWorker] {e}")
            self.signal_result.emit((False, [], []))


class CatalogueWriteWorker(QObject):
    """
    Writes <name>_img.dshow and <name>_all.catalogue into kaivo_dir,
    then writes the updated tilvista_dirs.json.

    The JSON entry for *name* will reference the filenames, not inline lists.
    """
    signal_result = pyqtSignal(bool)

    def __init__(self, kaivo_dir: str, db_path: str, db_data: dict,
                 entry_name: str, image_files: list, all_files: list):
        super().__init__()
        self.kaivo_dir   = kaivo_dir
        self.db_path     = db_path
        self.db_data     = db_data          # shared dict – will be mutated
        self.entry_name  = entry_name
        self.image_files = image_files
        self.all_files   = all_files

    def run(self):
        try:
            os.makedirs(self.kaivo_dir, exist_ok=True)
            safe = _safe_filename(self.entry_name)
            img_fname = f"{safe}_img.dshow"
            all_fname = f"{safe}_all.catalogue"

            # Write catalogue files (plain text, one path per line)
            with open(os.path.join(self.kaivo_dir, img_fname),
                      "w", encoding="utf-8") as f:
                f.write("\n".join(self.image_files))

            with open(os.path.join(self.kaivo_dir, all_fname),
                      "w", encoding="utf-8") as f:
                f.write("\n".join(self.all_files))

            # Patch the corresponding entry to store filenames, not lists
            for entry in self.db_data.get("entries", []):
                if entry["name"] == self.entry_name:
                    entry["image_files"] = img_fname
                    entry["all_files"]   = all_fname
                    break

            # Write index JSON
            with open(self.db_path, "w", encoding="utf-8") as f:
                json.dump(self.db_data, f, indent=2, ensure_ascii=False)

            self.signal_result.emit(True)
        except Exception as e:
            print(f"[CatalogueWriteWorker] {e}")
            self.signal_result.emit(False)


class DBSaveWorker(QObject):
    """Writes only tilvista_dirs.json (no catalogue files). Used for deletions."""
    signal_result = pyqtSignal(bool)

    def __init__(self, db_path: str, data: dict):
        super().__init__()
        self.db_path = db_path
        self.data    = data

    def run(self):
        try:
            os.makedirs(os.path.dirname(self.db_path), exist_ok=True)
            with open(self.db_path, "w", encoding="utf-8") as f:
                json.dump(self.data, f, indent=2, ensure_ascii=False)
            self.signal_result.emit(True)
        except Exception as e:
            print(f"[DBSaveWorker] {e}")
            self.signal_result.emit(False)


class DBLoadWorker(QObject):
    signal_result = pyqtSignal(tuple)  # (ok, dict)

    def __init__(self, db_path: str):
        super().__init__()
        self.db_path = db_path

    def run(self):
        try:
            with open(self.db_path, "r", encoding="utf-8") as f:
                data = json.load(f)
            self.signal_result.emit((True, data))
        except Exception:
            self.signal_result.emit((False, {}))


class CatalogueLoadWorker(QObject):
    """
    Reads <name>_img.dshow and <name>_all.catalogue from kaivo_dir.
    Returns (ok, image_files, all_files) as lists of path strings.
    """
    signal_result = pyqtSignal(tuple)   # (ok, list[str], list[str])

    def __init__(self, kaivo_dir: str, img_fname: str, all_fname: str):
        super().__init__()
        self.kaivo_dir = kaivo_dir
        self.img_fname = img_fname
        self.all_fname = all_fname

    @staticmethod
    def _read_lines(path: str) -> list:
        if not os.path.isfile(path):
            return []
        with open(path, "r", encoding="utf-8") as f:
            return [ln.strip() for ln in f if ln.strip()]

    def run(self):
        try:
            image_files = self._read_lines(
                os.path.join(self.kaivo_dir, self.img_fname))
            all_files   = self._read_lines(
                os.path.join(self.kaivo_dir, self.all_fname))
            self.signal_result.emit((True, image_files, all_files))
        except Exception as e:
            print(f"[CatalogueLoadWorker] {e}")
            self.signal_result.emit((False, [], []))


class VideoFramesWorker(QObject):
    """
    Extracts frames at t=0,10,20,… seconds via ffmpeg into a temp dir.
    Used as fallback when QMediaPlayer is unavailable.
    """
    signal_result = pyqtSignal(tuple)  # (ok, list[str])  paths to frame jpegs

    FRAME_TIMES = [0, 10, 20, 30, 40, 50]

    def __init__(self, path: str):
        super().__init__()
        self.path    = path
        self.tmpdir  = tempfile.mkdtemp(prefix="tilvista_vid_")

    def run(self):
        if not HAS_FFMPEG:
            self.signal_result.emit((False, []))
            return
        frame_paths = []
        for t in self.FRAME_TIMES:
            out = os.path.join(self.tmpdir, f"f{t:03d}.jpg")
            try:
                r = subprocess.run(
                    ["ffmpeg", "-i", self.path,
                     "-ss", str(t), "-vframes", "1",
                     "-f", "image2", "-y", out],
                    capture_output=True, timeout=10,
                )
                if r.returncode == 0 and os.path.isfile(out):
                    frame_paths.append(out)
            except Exception as e:
                print(f"[VideoFramesWorker] t={t}: {e}")
        self.signal_result.emit((bool(frame_paths), frame_paths))


# ════════════════════════════════════════════════════════════════════════════
#  PROGRESS BAR HELPERS
# ════════════════════════════════════════════════════════════════════════════

def make_progress_bar(parent=None) -> QProgressBar:
    pb = QProgressBar(parent)
    pb.setRange(0, 100)
    pb.setValue(0)
    pb.setFixedHeight(7)
    pb.setTextVisible(False)
    pb.setVisible(False)
    return pb


def progress_start(pb: QProgressBar, indeterminate: bool = False):
    pb.setRange(0, 0) if indeterminate else pb.setRange(0, 100)
    pb.setValue(0)
    pb.setVisible(True)


def progress_done(pb: QProgressBar):
    pb.setRange(0, 100)
    pb.setValue(100)
    pb.setVisible(False)


# ════════════════════════════════════════════════════════════════════════════
#  SCALABLE PREVIEW LABEL  (image / icon)
# ════════════════════════════════════════════════════════════════════════════

class PreviewLabel(QLabel):
    """QLabel that rescales its pixmap on every resize."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self._src: QPixmap | None = None
        self.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.setSizePolicy(QSizePolicy.Policy.Expanding,
                           QSizePolicy.Policy.Expanding)
        self.setMinimumSize(60, 60)
        self.setStyleSheet(
            "border: 1px solid #555; background: #1a1a1a; border-radius: 4px;")

    def set_source_pixmap(self, pm: QPixmap):
        self._src = pm
        self._repaint()

    def clear_preview(self):
        self._src = None
        self.clear()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self._repaint()

    def _repaint(self):
        if self._src and not self._src.isNull():
            scaled = self._src.scaled(
                self.size() - QSize(8, 8),
                Qt.AspectRatioMode.KeepAspectRatio,
                Qt.TransformationMode.SmoothTransformation,
            )
            self.setPixmap(scaled)


# ════════════════════════════════════════════════════════════════════════════
#  VIDEO PREVIEW WIDGET
#
#  Priority:
#    1. QMediaPlayer (muted, looping)       – needs PyQt6-Qt6 Multimedia
#    2. ffmpeg frame-cycling every 10 s     – needs ffmpeg in PATH
#    3. System icon fallback                – always available
# ════════════════════════════════════════════════════════════════════════════

FRAME_CYCLE_MS = 10_000   # ms between frames in fallback mode


class VideoPreviewWidget(QStackedWidget):
    """
    Transparent container that switches between a QVideoWidget (page 0)
    and a PreviewLabel showing cycling ffmpeg frames (page 1).
    """
    status_text = pyqtSignal(str)   # human-readable status for the type label

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setSizePolicy(QSizePolicy.Policy.Expanding,
                           QSizePolicy.Policy.Expanding)

        # ── Page 0: real video playback ────────────────────────────────
        if HAS_MULTIMEDIA:
            self._video_widget = QVideoWidget()
            self._player       = QMediaPlayer()
            self._audio        = QAudioOutput()
            self._audio.setVolume(0.0)          # muted preview
            self._player.setAudioOutput(self._audio)
            self._player.setVideoOutput(self._video_widget)
            self._player.setLoops(QMediaPlayer.Loops.Infinite)
            self.addWidget(self._video_widget)  # index 0
        else:
            placeholder = QLabel("(no multimedia)")
            placeholder.setAlignment(Qt.AlignmentFlag.AlignCenter)
            self.addWidget(placeholder)         # index 0 (unused)

        # ── Page 1: frame-cycling label ────────────────────────────────
        self._frame_label  = PreviewLabel()
        self.addWidget(self._frame_label)       # index 1

        # frame-cycling state
        self._frame_paths: list[str] = []
        self._frame_idx   = 0
        self._tmpdir: str | None = None
        self._cycle_timer = QTimer(self)
        self._cycle_timer.timeout.connect(self._next_frame)
        self._cycle_timer.setInterval(FRAME_CYCLE_MS)

        self._vid_thread: QThread | None = None
        self._vid_worker: QObject | None = None

        self.setCurrentIndex(1)  # start with label page

    # ── Public ────────────────────────────────────────────────────────
    def play(self, path: str):
        self.stop()
        self._cleanup_tmpdir()

        if HAS_MULTIMEDIA:
            self.setCurrentIndex(0)
            self._player.setSource(QUrl.fromLocalFile(path))
            self._player.play()
            self.status_text.emit("Video  (QMediaPlayer – muted)")
        elif HAS_FFMPEG:
            self.setCurrentIndex(1)
            self.status_text.emit("Video  – extracting frames…")
            w = VideoFramesWorker(path)
            self._tmpdir = w.tmpdir
            self._vid_thread, self._vid_worker = start_threaded_worker(
                w, self._on_frames_ready)
        else:
            self.setCurrentIndex(1)
            self.status_text.emit("Video  (no preview – install ffmpeg or PyQt6-Qt6)")

    def stop(self):
        self._cycle_timer.stop()
        if HAS_MULTIMEDIA:
            self._player.stop()
        if self._vid_thread:
            self._vid_thread.quit()
            self._vid_thread = None
            self._vid_worker = None

    def show_pixmap(self, pm: QPixmap, label: str = ""):
        """Display a static pixmap (image or icon) instead of video."""
        self.stop()
        self._cleanup_tmpdir()
        self.setCurrentIndex(1)
        self._frame_label.set_source_pixmap(pm)
        self.status_text.emit(label)

    def clear_preview(self):
        self.stop()
        self._cleanup_tmpdir()
        self.setCurrentIndex(1)
        self._frame_label.clear_preview()
        self.status_text.emit("")

    # ── Frame cycling ─────────────────────────────────────────────────
    def _on_frames_ready(self, result: tuple):
        ok, paths = result
        if self._vid_thread:
            cleanup_thread(self._vid_thread, self._vid_worker)
            self._vid_thread = None
            self._vid_worker = None
        if ok and paths:
            self._frame_paths = paths
            self._frame_idx   = 0
            self._show_frame(0)
            self._cycle_timer.start()
            self.status_text.emit(
                f"Video  (ffmpeg frame-cycle, {FRAME_CYCLE_MS // 1000} s interval,"
                f" {len(paths)} frames)")
        else:
            self.status_text.emit("Video  (frame extraction failed)")

    def _next_frame(self):
        if not self._frame_paths:
            return
        self._frame_idx = (self._frame_idx + 1) % len(self._frame_paths)
        self._show_frame(self._frame_idx)

    def _show_frame(self, idx: int):
        pm = QPixmap(self._frame_paths[idx])
        if not pm.isNull():
            self._frame_label.set_source_pixmap(pm)

    def _cleanup_tmpdir(self):
        if self._tmpdir and os.path.isdir(self._tmpdir):
            shutil.rmtree(self._tmpdir, ignore_errors=True)
        self._tmpdir = None
        self._frame_paths = []


# ════════════════════════════════════════════════════════════════════════════
#  DIRECTORY DATABASE PANEL
# ════════════════════════════════════════════════════════════════════════════

class DirDatabasePanel(QWidget):
    """
    Persistent directory store under optegnelser/Kaivo/.

    File layout:
        tilvista_dirs.json            ← index, one small entry per directory
        <safe_name>_img.dshow         ← image paths, one per line
        <safe_name>_all.catalogue     ← all file paths, one per line

    The JSON entry references the catalogue filenames – the lists themselves
    are never stored inside the JSON.

    Signals:
        dir_loaded(str, list, list):  (abs_path, image_files, all_files)
            file lists are [] when no catalogue files exist yet.
    """
    dir_loaded = pyqtSignal(str, list, list)

    def __init__(self, get_current_dir, parent=None):
        super().__init__(parent)
        self._get_dir      = get_current_dir
        self._base         = _script_dir()
        self._kaivo        = _kaivo_dir()
        self._db_path      = _kaivo_db_path()
        self._db           = {"entries": []}
        self._thread       = None
        self._worker       = None
        self._cat_thread   = None          # catalogue load thread
        self._cat_worker   = None
        self._pending_load_path = ""       # set while catalogue load is running
        self._pending_cache: dict = {}     # image_files / all_files before Save
        self._build_ui()
        self._load_db()

    # ── UI ────────────────────────────────────────────────────────────
    def _build_ui(self):
        lv = QVBoxLayout(self)
        lv.setContentsMargins(6, 6, 6, 6)
        lv.setSpacing(4)

        hdr = QLabel("📁  Kaivo  –  Directory Store")
        hdr.setStyleSheet("font-weight: bold; font-size: 11px;")
        lv.addWidget(hdr)

        self.line_name = QLineEdit()
        self.line_name.setPlaceholderText("Entry name (auto-filled after scan)…")
        lv.addWidget(self.line_name)

        self.list_widget = QListWidget()
        self.list_widget.setSelectionMode(
            QAbstractItemView.SelectionMode.SingleSelection)
        self.list_widget.setToolTip("Double-click → Load from DB")
        self.list_widget.itemDoubleClicked.connect(self._on_double_click)
        lv.addWidget(self.list_widget)

        # Per-entry info: filenames + scan timestamp
        self.lbl_entry_info = QLabel("")
        self.lbl_entry_info.setStyleSheet("font-size: 9px; color: #888;")
        self.lbl_entry_info.setWordWrap(True)
        self.list_widget.currentTextChanged.connect(self._update_entry_info)
        lv.addWidget(self.lbl_entry_info)

        self.pb = make_progress_bar()
        lv.addWidget(self.pb)

        btn_row = QHBoxLayout()
        self.btn_save   = QPushButton("Save to DB")
        self.btn_load   = QPushButton("Load from DB")
        self.btn_delete = QPushButton("Delete from DB")
        for b in (self.btn_save, self.btn_load, self.btn_delete):
            btn_row.addWidget(b)
        self.btn_save.clicked.connect(self.save_current_dir)
        self.btn_load.clicked.connect(self.load_selected)
        self.btn_delete.clicked.connect(self.delete_selected)
        lv.addLayout(btn_row)

        self.lbl_status = QLabel("")
        self.lbl_status.setStyleSheet("font-size: 10px; color: gray;")
        lv.addWidget(self.lbl_status)

    # ── Public: called by AleaVueTab after a scan ─────────────────────
    def update_cache(self, path: str, image_files: list, all_files: list):
        """
        Pre-fills name field and, if the path already has a DB entry,
        immediately writes fresh catalogue files for it.
        Otherwise stashes data in _pending_cache until user clicks Save.
        """
        self.line_name.setText(auto_entry_name(path))

        try:
            stored = os.path.relpath(path, self._base)
        except ValueError:
            stored = path

        # Does this path already have an entry?
        for entry in self._db["entries"]:
            resolved = os.path.normpath(
                os.path.join(self._base, entry["path"]))
            if resolved == os.path.normpath(path):
                entry["scanned_at"] = datetime.now().isoformat(timespec="seconds")
                self._write_catalogues_and_json(
                    entry["name"], image_files, all_files)
                self._update_entry_info(entry["name"])
                return

        # New path – hold for Save
        self._pending_cache = {
            "path":        stored,
            "image_files": image_files,
            "all_files":   all_files,
        }

    def save_current_dir(self):
        path = self._get_dir()
        if not path or not os.path.isdir(path):
            self.lbl_status.setText("⚠  No valid directory active.")
            return
        name = self.line_name.text().strip() or auto_entry_name(path)
        self.line_name.setText(name)

        try:
            stored = os.path.relpath(path, self._base)
        except ValueError:
            stored = path

        pending     = self._pending_cache
        safe        = _safe_filename(name)
        img_fname   = f"{safe}_img.dshow"
        all_fname   = f"{safe}_all.catalogue"
        now_ts      = datetime.now().isoformat(timespec="seconds")

        # Update-or-insert in index
        for entry in self._db["entries"]:
            if entry["name"] == name:
                entry["path"]        = stored
                entry["image_files"] = img_fname
                entry["all_files"]   = all_fname
                entry["scanned_at"]  = now_ts
                break
        else:
            self._db["entries"].append({
                "name":        name,
                "path":        stored,
                "image_files": img_fname,
                "all_files":   all_fname,
                "scanned_at":  now_ts,
            })

        self._pending_cache = {}
        self._refresh_list()

        img_files = pending.get("image_files", [])
        all_files = pending.get("all_files",   [])
        self._write_catalogues_and_json(name, img_files, all_files)

    def load_selected(self):
        item = self.list_widget.currentItem()
        if item:
            self._emit_from_name(item.text())

    def delete_selected(self):
        item = self.list_widget.currentItem()
        if not item:
            return
        name = item.text()
        # Remove catalogue files
        for entry in self._db["entries"]:
            if entry["name"] == name:
                for key in ("image_files", "all_files"):
                    fname = entry.get(key, "")
                    if fname:
                        fpath = os.path.join(self._kaivo, fname)
                        try:
                            os.unlink(fpath)
                        except OSError:
                            pass
                break
        self._db["entries"] = [
            e for e in self._db["entries"] if e["name"] != name]
        # Write JSON only (no catalogue data involved)
        progress_start(self.pb, indeterminate=True)
        w = DBSaveWorker(self._db_path, self._db)
        self._thread, self._worker = start_threaded_worker(
            w, self._on_save_done)
        self._refresh_list()
        self.lbl_entry_info.setText("")

    # ── Private ───────────────────────────────────────────────────────
    def _on_double_click(self, item):
        self._emit_from_name(item.text())

    def _emit_from_name(self, name: str):
        """
        Resolve the directory path, then load catalogue files (threaded)
        before emitting dir_loaded.
        """
        for entry in self._db["entries"]:
            if entry["name"] == name:
                abs_path = os.path.normpath(
                    os.path.join(self._base, entry["path"]))
                if not os.path.isdir(abs_path):
                    QMessageBox.warning(
                        self, "Path not found",
                        f"Could not resolve:\n{abs_path}\n\n"
                        f"(Stored: {entry['path']})")
                    return
                img_fname = entry.get("image_files", "")
                all_fname = entry.get("all_files",   "")
                if img_fname and all_fname:
                    self._pending_load_path = abs_path
                    progress_start(self.pb, indeterminate=True)
                    w = CatalogueLoadWorker(self._kaivo, img_fname, all_fname)
                    self._cat_thread, self._cat_worker = start_threaded_worker(
                        w, self._on_catalogue_loaded)
                else:
                    # No catalogue files yet → emit with empty lists (triggers scan)
                    self.dir_loaded.emit(abs_path, [], [])
                self.lbl_status.setText(f"Loading: {abs_path} …")
                return

    def _on_catalogue_loaded(self, result: tuple):
        ok, image_files, all_files = result
        progress_done(self.pb)
        cleanup_thread(self._cat_thread, self._cat_worker)
        self._cat_thread = None
        self._cat_worker = None
        n = len(image_files)
        self.lbl_status.setText(
            f"✓  {n} images loaded from catalogue." if ok
            else "⚠  Catalogue read error – will rescan.")
        self.dir_loaded.emit(self._pending_load_path, image_files, all_files)

    def _update_entry_info(self, name: str):
        for entry in self._db["entries"]:
            if entry["name"] == name:
                img_f = entry.get("image_files", "—")
                all_f = entry.get("all_files",   "—")
                ts    = entry.get("scanned_at",  "—")
                self.lbl_entry_info.setText(
                    f"🖼 {img_f}\n📄 {all_f}\n⏱ {ts}")
                return
        self.lbl_entry_info.setText("")

    def _refresh_list(self):
        cur      = self.list_widget.currentItem()
        cur_text = cur.text() if cur else ""
        self.list_widget.clear()
        for e in self._db["entries"]:
            self.list_widget.addItem(e["name"])
        items = self.list_widget.findItems(cur_text, Qt.MatchFlag.MatchExactly)
        if items:
            self.list_widget.setCurrentItem(items[0])

    # ── Write helpers ─────────────────────────────────────────────────
    def _write_catalogues_and_json(self, name: str,
                                   image_files: list, all_files: list):
        """Launch CatalogueWriteWorker (writes .dshow + .catalogue + JSON)."""
        progress_start(self.pb, indeterminate=True)
        self.btn_save.setEnabled(False)
        w = CatalogueWriteWorker(
            self._kaivo, self._db_path,
            self._db, name, image_files, all_files)
        self._thread, self._worker = start_threaded_worker(
            w, self._on_save_done)

    def _on_save_done(self, ok: bool):
        progress_done(self.pb)
        self.btn_save.setEnabled(True)
        self.lbl_status.setText("✓  Saved." if ok else "✗  Save failed.")
        cleanup_thread(self._thread, self._worker)

    # ── Initial load ──────────────────────────────────────────────────
    def _load_db(self):
        progress_start(self.pb, indeterminate=True)
        w = DBLoadWorker(self._db_path)
        self._thread, self._worker = start_threaded_worker(
            w, self._on_load_done)

    def _on_load_done(self, result: tuple):
        ok, data = result
        progress_done(self.pb)
        if ok and isinstance(data, dict) and "entries" in data:
            self._db = data
        self._refresh_list()
        cleanup_thread(self._thread, self._worker)


# ════════════════════════════════════════════════════════════════════════════
#  ALEAVUE TAB  (Slideshow)
# ════════════════════════════════════════════════════════════════════════════

class AleaVueTab(QWidget):
    request_dir_change = pyqtSignal(str)

    def __init__(self, get_global_dir, parent=None):
        super().__init__(parent)
        self._get_global_dir  = get_global_dir
        self._image_paths: list[str] = []
        self._all_files:   list[str] = []
        self._last_scanned_path = ""
        self._slideshow_window = None
        self._scan_thread = None
        self._scan_worker = None
        self._build_ui()

    def _build_ui(self):
        outer = QHBoxLayout(self)

        left = QWidget()
        lv   = QVBoxLayout(left)
        lv.setSpacing(6)

        self.lbl_status = QLabel("No directory selected.")
        self.lbl_status.setWordWrap(True)
        lv.addWidget(self.lbl_status)

        self.pb = make_progress_bar()
        lv.addWidget(self.pb)

        self.btn_load_dir = QPushButton("📂  Load from Directory")
        self.btn_load_dir.setToolTip("Scan selected directory for images")
        self.btn_load_dir.clicked.connect(self._load_from_directory)
        lv.addWidget(self.btn_load_dir)

        self.check_fullscreen = QCheckBox("Fullscreen")
        self.check_fullscreen.setChecked(True)
        lv.addWidget(self.check_fullscreen)

        self.btn_start = QPushButton("▶  Start AleaVue")
        self.btn_start.setFixedHeight(48)
        self.btn_start.clicked.connect(self._start_slideshow)
        lv.addWidget(self.btn_start)

        lv.addStretch()
        outer.addWidget(left, 1)

        self._db_panel = DirDatabasePanel(self._get_global_dir)
        self._db_panel.dir_loaded.connect(self._on_db_dir_loaded)
        outer.addWidget(self._db_panel, 1)

    # ── MainWindow callback ───────────────────────────────────────────
    def on_directory_changed(self, path: str):
        self._image_paths = []
        self._all_files   = []
        self.lbl_status.setText(f"Dir set: {path}")

    # ── Load from Directory button ────────────────────────────────────
    def _load_from_directory(self):
        path = self._get_global_dir()
        if not path or not os.path.isdir(path):
            QMessageBox.warning(self, "No Directory",
                                "Please select a directory first.")
            return
        self._scan_dir(path)

    # ── DB panel callback: may carry cached file lists ────────────────
    def _on_db_dir_loaded(self, path: str,
                           image_files: list, all_files: list):
        self.request_dir_change.emit(path)
        if image_files:
            # Use cached lists – no re-scan needed
            self._image_paths = image_files
            self._all_files   = all_files
            self._last_scanned_path = path
            self.lbl_status.setText(
                f"✓  {len(image_files)} images (from cache)  –  {path}")
        else:
            self.lbl_status.setText(f"Loaded from DB: {path}  –  scanning…")
            self._scan_dir(path)

    # ── Scan ──────────────────────────────────────────────────────────
    def _scan_dir(self, path: str):
        progress_start(self.pb, indeterminate=False)
        self.btn_start.setEnabled(False)
        self.btn_load_dir.setEnabled(False)
        self.lbl_status.setText(f"Scanning: {path} …")
        self._last_scanned_path = path
        w = ScanWorker(path)
        w.signal_progress.connect(self.pb.setValue)
        self._scan_thread, self._scan_worker = start_threaded_worker(
            w, self._on_scan_done)

    def _on_scan_done(self, result: tuple):
        ok, image_files, all_files = result
        progress_done(self.pb)
        self.btn_start.setEnabled(True)
        self.btn_load_dir.setEnabled(True)
        cleanup_thread(self._scan_thread, self._scan_worker)
        if ok:
            self._image_paths = image_files
            self._all_files   = all_files
            self.lbl_status.setText(
                f"✓  {len(image_files)} images found.")
            # Pre-fill DB name field; update cache if entry already exists
            self._db_panel.update_cache(
                self._last_scanned_path, image_files, all_files)
        else:
            self.lbl_status.setText("✗  Scan failed.")

    # ── Start slideshow ───────────────────────────────────────────────
    def _start_slideshow(self):
        directory = self._get_global_dir()
        if not directory:
            QMessageBox.warning(self, "No Directory",
                                "Please select a directory first.")
            return
        if self._image_paths:
            self._open_window(directory, self._image_paths)
        else:
            # lazy scan
            progress_start(self.pb, indeterminate=False)
            self.btn_start.setEnabled(False)
            self.btn_load_dir.setEnabled(False)
            w = ScanWorker(directory)
            w.signal_progress.connect(self.pb.setValue)
            self._scan_thread, self._scan_worker = start_threaded_worker(
                w, self._on_scan_then_open)

    def _on_scan_then_open(self, result: tuple):
        ok, image_files, all_files = result
        progress_done(self.pb)
        self.btn_start.setEnabled(True)
        self.btn_load_dir.setEnabled(True)
        cleanup_thread(self._scan_thread, self._scan_worker)
        if not (ok and image_files):
            QMessageBox.warning(self, "No Images",
                                "No image files found in the selected directory.")
            return
        self._image_paths = image_files
        self._all_files   = all_files
        self._db_panel.update_cache(self._get_global_dir(), image_files, all_files)
        self._open_window(self._get_global_dir(), image_files)

    def _open_window(self, directory: str, paths: list):
        self._slideshow_window = SlideshowWindow(
            directory, paths, _log_dir(), self._log_enabled())
        if self.check_fullscreen.isChecked():
            self._slideshow_window.showFullScreen()
        else:
            self._slideshow_window.show()

    def _log_enabled(self) -> bool:
        return False   # extend via settings later


# ════════════════════════════════════════════════════════════════════════════
#  SATTUMAPIC TAB  (Random File Opener)
# ════════════════════════════════════════════════════════════════════════════

class SattumaPicTab(QWidget):

    def __init__(self, get_global_dir, parent=None):
        super().__init__(parent)
        self._get_global_dir = get_global_dir
        self._random_file    = ""
        self._all_files: list[str] = []
        self._scan_thread    = None
        self._scan_worker    = None
        self._build_ui()

    def _build_ui(self):
        outer = QHBoxLayout(self)

        # ── Left: controls ─────────────────────────────────────────────
        left = QWidget()
        left.setMaximumWidth(360)
        lv = QVBoxLayout(left)
        lv.setSpacing(6)

        row1 = QHBoxLayout()
        self.btn_random   = QPushButton("🎲  Random File")
        self.chk_autopick = QCheckBox("Auto-pick on dir select")
        self.btn_random.clicked.connect(self._pick_random)
        row1.addWidget(self.btn_random)
        row1.addWidget(self.chk_autopick)
        lv.addLayout(row1)

        self.lbl_file = QLabel("No file selected.")
        self.lbl_file.setWordWrap(True)
        self.lbl_file.setStyleSheet("font-size: 10px; color: gray;")
        lv.addWidget(self.lbl_file)

        self.pb = make_progress_bar()
        lv.addWidget(self.pb)

        row2 = QHBoxLayout()
        self.btn_open_file = QPushButton("📂  Open File")
        self.chk_autoopen  = QCheckBox("Auto-open on random")
        self.btn_open_file.clicked.connect(self._open_file)
        self.btn_open_file.setEnabled(False)
        row2.addWidget(self.btn_open_file)
        row2.addWidget(self.chk_autoopen)
        lv.addLayout(row2)

        self.btn_open_dir = QPushButton("📁  Open File's Directory")
        self.btn_open_dir.clicked.connect(self._open_file_dir)
        self.btn_open_dir.setEnabled(False)
        lv.addWidget(self.btn_open_dir)

        lv.addStretch()
        outer.addWidget(left)

        # ── Right: unified preview (image + video) ─────────────────────
        right = QWidget()
        rv = QVBoxLayout(right)
        rv.setContentsMargins(4, 4, 4, 4)
        rv.setSpacing(4)

        hdr = QLabel("Preview")
        hdr.setAlignment(Qt.AlignmentFlag.AlignCenter)
        hdr.setStyleSheet("font-weight: bold;")
        rv.addWidget(hdr)

        self.preview = VideoPreviewWidget()
        rv.addWidget(self.preview, 1)

        self.lbl_type = QLabel("")
        self.lbl_type.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.lbl_type.setStyleSheet("font-size: 10px; color: gray;")
        self.preview.status_text.connect(self.lbl_type.setText)
        rv.addWidget(self.lbl_type)

        outer.addWidget(right, 1)

    # ── MainWindow / AleaVue callbacks ────────────────────────────────
    def on_directory_changed(self, path: str, all_files: list | None = None):
        if all_files is not None:
            self._all_files = all_files
        else:
            self._all_files = []
        if self.chk_autopick.isChecked():
            if self._all_files:
                self._pick_random()
            else:
                self._scan_and_pick(path)

    # ── Scan ──────────────────────────────────────────────────────────
    def _scan_and_pick(self, path: str):
        progress_start(self.pb, indeterminate=False)
        w = ScanWorker(path)
        w.signal_progress.connect(self.pb.setValue)
        self._scan_thread, self._scan_worker = start_threaded_worker(
            w, self._on_scan_done)

    def _on_scan_done(self, result: tuple):
        ok, image_files, all_files = result
        progress_done(self.pb)
        cleanup_thread(self._scan_thread, self._scan_worker)
        if ok:
            self._all_files = all_files
            self._pick_random()

    # ── Pick random ───────────────────────────────────────────────────
    def _pick_random(self):
        directory = self._get_global_dir()
        if not directory:
            QMessageBox.warning(self, "No Directory",
                                "Please select a directory first.")
            return
        if not self._all_files:
            self._scan_and_pick(directory)
            return
        self._random_file = random.choice(self._all_files)
        self.lbl_file.setText(self._random_file)
        self.btn_open_file.setEnabled(True)
        self.btn_open_dir.setEnabled(True)
        self._update_preview(self._random_file)
        if self.chk_autoopen.isChecked():
            self._open_file()

    # ── Preview ───────────────────────────────────────────────────────
    def _update_preview(self, path: str):
        ext = Path(path).suffix.lower()

        # 1) Image
        if ext in IMAGE_SUFFIXES:
            reader = QImageReader(path)
            orig   = reader.size()
            if orig.isValid() and (orig.width() > 1024 or orig.height() > 1024):
                scale = min(1024 / orig.width(), 1024 / orig.height())
                reader.setScaledSize(QSize(int(orig.width() * scale),
                                           int(orig.height() * scale)))
            img = reader.read()
            if not img.isNull():
                self.preview.show_pixmap(QPixmap.fromImage(img),
                                         f"Image  {ext}")
                return

        # 2) Video
        if ext in VIDEO_SUFFIXES:
            self.preview.play(path)
            return

        # 3) System icon fallback
        fip  = QFileIconProvider()
        icon = fip.icon(QFileInfo(path))
        if not icon.isNull():
            self.preview.show_pixmap(icon.pixmap(QSize(128, 128)),
                                     f"File  {ext or '(no ext)'}")
        else:
            self.preview.clear_preview()

    # ── Open actions ──────────────────────────────────────────────────
    def _open_file(self):
        if self._random_file:
            try:
                open_path(self._random_file)
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Could not open:\n{e}")

    def _open_file_dir(self):
        if self._random_file:
            open_path(os.path.dirname(os.path.abspath(self._random_file)))


# ════════════════════════════════════════════════════════════════════════════
#  GLOBAL DIRECTORY BAR
# ════════════════════════════════════════════════════════════════════════════

class DirBar(QWidget):
    directory_changed = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 4, 0, 4)

        self.line_edit = QLineEdit()
        self.line_edit.setPlaceholderText("Select or type a directory path…")
        self.line_edit.editingFinished.connect(self._on_text_committed)

        self.btn_browse = QPushButton("Browse")
        self.btn_browse.setFixedWidth(80)
        self.btn_browse.clicked.connect(self._browse)

        layout.addWidget(self.line_edit)
        layout.addWidget(self.btn_browse)

    def _browse(self):
        path = QFileDialog.getExistingDirectory(self, "Select Directory")
        if path:
            self.set_directory(path)
            self.directory_changed.emit(path)

    def _on_text_committed(self):
        path = self.line_edit.text().strip()
        if os.path.isdir(path):
            self.directory_changed.emit(path)

    def get_directory(self) -> str:
        return self.line_edit.text().strip()

    def set_directory(self, path: str):
        self.line_edit.setText(path)


# ════════════════════════════════════════════════════════════════════════════
#  MAIN WINDOW
# ════════════════════════════════════════════════════════════════════════════

class TilVistaApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("TilVista  –  v0.4.0")
        self.setMinimumSize(780, 540)

        central = QWidget()
        self.setCentralWidget(central)
        mv = QVBoxLayout(central)
        mv.setContentsMargins(8, 8, 8, 8)
        mv.setSpacing(4)

        self._dir_bar = DirBar()
        self._dir_bar.directory_changed.connect(self._on_dir_changed)
        mv.addWidget(self._dir_bar)

        self._tabs = QTabWidget()
        self._aleavue_tab    = AleaVueTab(self._dir_bar.get_directory)
        self._sattumapic_tab = SattumaPicTab(self._dir_bar.get_directory)

        # AleaVue DB panel → sync dir bar + notify SattumaPic (with cached files)
        self._aleavue_tab.request_dir_change.connect(self._on_dir_from_db)
        self._aleavue_tab._db_panel.dir_loaded.connect(self._on_db_files_loaded)

        self._tabs.addTab(self._aleavue_tab,    "🖼   AleaVue  (Slideshow)")
        self._tabs.addTab(self._sattumapic_tab, "🎲   SattumaPic  (Random File)")
        mv.addWidget(self._tabs)

    def _on_dir_changed(self, path: str):
        self._aleavue_tab.on_directory_changed(path)
        self._sattumapic_tab.on_directory_changed(path)

    def _on_dir_from_db(self, path: str):
        """DB loaded a dir → update bar; SattumaPic handled via _on_db_files_loaded."""
        self._dir_bar.set_directory(path)

    def _on_db_files_loaded(self, path: str, image_files: list, all_files: list):
        """Pass cached all_files to SattumaPic to avoid redundant scan."""
        self._sattumapic_tab.on_directory_changed(path, all_files)


# ════════════════════════════════════════════════════════════════════════════
#  SLIDESHOW WINDOW  (AleaVue fullscreen player)
# ════════════════════════════════════════════════════════════════════════════

class SlideshowWindow(QMainWindow):
    def __init__(self, directory: str, image_paths: list,
                 logpath: str, log: bool):
        super().__init__()
        self.setWindowTitle("TilVista · AleaVue")
        self.setStyleSheet("background-color: black;")

        self._log_path    = logpath
        self._log         = log
        self._image_paths = image_paths
        self._image_order: list[str] = []
        self._backtrack   = 0
        self._current     = ""

        self.setCursor(Qt.CursorShape.BlankCursor)
        _prevent_sleep()

        screen = QApplication.primaryScreen().availableGeometry()
        self._show_w     = screen.width()
        self._show_h     = screen.height()
        self._show_ratio = self._show_w / self._show_h

        self._dt    = 4500
        self._timer = QTimer(self)
        self._timer.timeout.connect(self.change_image)
        self._timer.setInterval(self._dt)
        self._timer.start()

        self._label = QLabel()
        self._label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._label.setStyleSheet("background-color: black;")

        container = QWidget()
        layout = QVBoxLayout(container)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self._label)
        self.setCentralWidget(container)

        self.change_image()

    def resizeEvent(self, event):
        s = event.size()
        self._show_w     = s.width()
        self._show_h     = s.height()
        self._show_ratio = self._show_w / self._show_h

    def change_image(self):
        if not self._image_paths:
            return
        if self._backtrack >= -1:
            self._backtrack = 0
            img = random.choice(self._image_paths)
            self._image_order.append(img)
            self._display_image(img)
        else:
            self._backtrack += 1
            try:
                self._display_image(self._image_order[self._backtrack])
            except IndexError:
                self._backtrack = 0
                self.change_image()

    def _display_image(self, path: str):
        self._current = path
        reader = QImageReader(path)
        orig   = reader.size()
        if orig.isValid() and (orig.width() > self._show_w
                                or orig.height() > self._show_h):
            scale = min(self._show_w / orig.width(),
                        self._show_h / orig.height())
            reader.setScaledSize(QSize(int(orig.width() * scale),
                                       int(orig.height() * scale)))
        image = reader.read()
        if image.isNull():
            self._log_error(path, reader.errorString())
            self.change_image()
            return
        pm = QPixmap.fromImage(image)
        if pm.width() / pm.height() > self._show_ratio:
            pm = pm.scaledToWidth(self._show_w,
                                  Qt.TransformationMode.SmoothTransformation)
        else:
            pm = pm.scaledToHeight(self._show_h - 18,
                                   Qt.TransformationMode.SmoothTransformation)
        self._label.setPixmap(pm)
        self._timer.stop()
        self._timer.start()
        _prevent_sleep()

    def _log_error(self, path: str, error: str):
        if self._log:
            os.makedirs(self._log_path, exist_ok=True)
            with open(os.path.join(self._log_path, "loadingerrors.log"), "a") as f:
                f.write(f"Failed: {path}  |  {error}\n")

    def keyPressEvent(self, event):
        key = event.key()
        if key == Qt.Key.Key_Escape:
            self.setCursor(Qt.CursorShape.ArrowCursor)
            _restore_sleep()
            self.close()
        elif key == Qt.Key.Key_Right:
            self.change_image()
        elif key == Qt.Key.Key_Left:
            self._go_back()
        elif key == Qt.Key.Key_Space:
            if self._timer.isActive():
                self._timer.stop()
            else:
                self._timer.setInterval(self._dt)
                self._timer.start()
        elif key == Qt.Key.Key_Return:
            self._save_bookmark(self._current)

    def _go_back(self):
        if not self._image_paths:
            return
        self._backtrack -= 1
        try:
            self._display_image(self._image_order[self._backtrack])
        except IndexError:
            self._backtrack += 1

    def _save_bookmark(self, path: str):
        os.makedirs(self._log_path, exist_ok=True)
        review_file = os.path.join(self._log_path, "reviewpics.txt")
        with open(review_file, "a+") as f:
            f.seek(0)
            existing = f.read().splitlines()
            if path not in existing:
                f.write(path + "\n")


# ════════════════════════════════════════════════════════════════════════════
#  ENTRY POINT
# ════════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = TilVistaApp()
    window.show()
    sys.exit(app.exec())