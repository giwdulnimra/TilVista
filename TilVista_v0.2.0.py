# -*- coding: utf-8 -*-
"""
╔═══════════════════════════════════════════════════════════════════╗
║  TilVista  –  Tilfeldig (nor.: zufällig) + Vista (span.: Blick)   ║
║  Sub-tools:  AleaVue  (lat. alea=Würfel + fr. vue=Blick)          ║
║                  → zufällige Bild-Slideshow                       ║
║              SattumaPic  (fin. sattuma=Zufall + Pic)              ║
║                  → zufälliger Datei-Öffner mit Vorschau           ║
╠═══════════════════════════════════════════════════════════════════╣
║  @author:  Ludwig, Armin                                          ║
║  @year:    2024-2025                                              ║
║  version:  v0.2.0                                                 ║
╚═══════════════════════════════════════════════════════════════════╝

dependencies:
    pip install PyQt6
    ffmpeg in PATH (optional, enables video thumbnails)

DB storage:
    <script_dir>/optegnelser/DataKaivo/tilvista_dirs.json
    DataKaivo  =  Finnish "data" + "kaivo" (Brunnen/Quelle) → Datenquelle

history:
    v0.2.0 – 2025 – AleaVue (lat./fr.) replaces SattumaBild
                  – DB path moved to optegnelser/DataKaivo/
                  – "Instant Load" checkbox → "Load from Directory" button
                  – DB panel buttons renamed: Save to DB / Load from DB / Delete from DB
                  – Preview label: scalable, fills available space (no fixed size)
                  – Auto DB-entry name: "<folder>_<yymmdd>"
                  – Video thumbnails via ffmpeg (optional, threaded, fallback to icon)
                  – Progress bars for all directory and DB I/O operations
    v0.1.0 – 2025 – merged RanChFO + RanSlidSHW
"""

import ctypes
import json
import os
import random
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path

from PyQt6.QtCore import (
    QFileInfo, QObject, QSize, QThread, Qt, QTimer, pyqtSignal,
)
from PyQt6.QtGui import QImageReader, QPixmap
from PyQt6.QtWidgets import (
    QAbstractItemView, QApplication, QCheckBox, QFileDialog,
    QFileIconProvider, QHBoxLayout, QLabel, QLineEdit, QListWidget,
    QMainWindow, QMessageBox, QProgressBar, QPushButton, QSizePolicy,
    QTabWidget, QVBoxLayout, QWidget,
)

# ─── Windows: sleep prevention ───────────────────────────────────────────────
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


# ─── DB path helper ───────────────────────────────────────────────────────────
def _db_path() -> str:
    """
    <script_dir>/optegnelser/DataKaivo/tilvista_dirs.json
    DataKaivo: Finnish kaivo = Brunnen/Quelle → 'Datenquelle'
    """
    base = os.path.dirname(os.path.abspath(sys.argv[0]))
    return os.path.join(base, "optegnelser", "DataKaivo", "tilvista_dirs.json")


# ─── Threading utility ────────────────────────────────────────────────────────
def start_threaded_worker(worker: QObject, result_slot) -> tuple:
    """
    Generic worker launcher (from Verbesserung_Threaded_Worker.txt pattern).
    Moves worker to a new QThread, wires run + signal_result, starts thread.
    Returns (thread, worker) – caller must hold references until cleanup.
    """
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


# ─── Auto-name for DB entries ─────────────────────────────────────────────────
def auto_entry_name(path: str) -> str:
    """Returns '<lowest_folder>_<yymmdd>', e.g. 'Fotos_250508'."""
    folder = os.path.basename(os.path.normpath(path)) or path
    date   = datetime.now().strftime("%y%m%d")
    return f"{folder}_{date}"


# ════════════════════════════════════════════════════════════════════════════
#  WORKERS
# ════════════════════════════════════════════════════════════════════════════

IMAGE_SUFFIXES = {
    ".jpg", ".jpeg", ".png", ".bmp",
    ".tif", ".tiff", ".gif", ".webp",
}

VIDEO_SUFFIXES = {
    ".mp4", ".mkv", ".avi", ".mov", ".wmv",
    ".flv", ".webm", ".m4v", ".ts", ".mts",
}


class ScanImageWorker(QObject):
    signal_result   = pyqtSignal(tuple)   # (True, list[str])
    signal_progress = pyqtSignal(int)     # 0-100

    def __init__(self, directory: str):
        super().__init__()
        self.directory = directory

    def run(self):
        paths = []
        try:
            # Two-pass: count first for accurate progress
            all_files = []
            for root, _, files in os.walk(self.directory):
                for f in files:
                    all_files.append(os.path.join(root, f))
            total = len(all_files) or 1
            for i, fp in enumerate(all_files):
                if Path(fp).suffix.lower() in IMAGE_SUFFIXES:
                    paths.append(fp)
                self.signal_progress.emit(int((i + 1) / total * 100))
            self.signal_result.emit((True, paths))
        except Exception as e:
            print(f"[ScanImageWorker] {e}")
            self.signal_result.emit((False, []))


class ScanAllFilesWorker(QObject):
    signal_result   = pyqtSignal(tuple)
    signal_progress = pyqtSignal(int)

    def __init__(self, directory: str):
        super().__init__()
        self.directory = directory

    def run(self):
        paths = []
        try:
            all_dirs = list(os.walk(self.directory))
            total_files = sum(len(files) for _, _, files in all_dirs) or 1
            count = 0
            for root, _, files in all_dirs:
                for f in files:
                    paths.append(os.path.join(root, f))
                    count += 1
                    self.signal_progress.emit(int(count / total_files * 100))
            self.signal_result.emit((True, paths))
        except Exception as e:
            print(f"[ScanAllFilesWorker] {e}")
            self.signal_result.emit((False, []))


class DBSaveWorker(QObject):
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
    signal_result = pyqtSignal(tuple)   # (success, data)

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


class VideoThumbnailWorker(QObject):
    """
    Extracts a single frame from a video using ffmpeg.
    Falls back gracefully if ffmpeg is not in PATH.
    """
    signal_result = pyqtSignal(tuple)   # (success: bool, pixmap | None)

    def __init__(self, path: str):
        super().__init__()
        self.path = path

    def run(self):
        import shutil
        if not shutil.which("ffmpeg"):
            self.signal_result.emit((False, None))
            return
        tmp_fd, tmp_path = tempfile.mkstemp(suffix=".jpg")
        os.close(tmp_fd)
        try:
            result = subprocess.run(
                ["ffmpeg", "-i", self.path,
                 "-vframes", "1", "-ss", "00:00:02",
                 "-f", "image2", "-y", tmp_path],
                capture_output=True, timeout=12,
            )
            if result.returncode == 0:
                pm = QPixmap(tmp_path)
                if not pm.isNull():
                    self.signal_result.emit((True, pm))
                    return
        except Exception as e:
            print(f"[VideoThumbnailWorker] {e}")
        finally:
            if os.path.exists(tmp_path):
                os.unlink(tmp_path)
        self.signal_result.emit((False, None))


# ════════════════════════════════════════════════════════════════════════════
#  SCALABLE PREVIEW LABEL
# ════════════════════════════════════════════════════════════════════════════

class PreviewLabel(QLabel):
    """
    QLabel that scales its pixmap to fill all available space
    while keeping aspect ratio.  No fixed size needed.
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        self._source_pm: QPixmap | None = None
        self.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.setSizePolicy(
            QSizePolicy.Policy.Expanding,
            QSizePolicy.Policy.Expanding,
        )
        self.setMinimumSize(80, 80)
        self.setStyleSheet(
            "border: 1px solid #555;"
            "background-color: #1a1a1a;"
            "border-radius: 4px;"
        )

    def set_source_pixmap(self, pm: QPixmap):
        self._source_pm = pm
        self._repaint_scaled()

    def clear_preview(self):
        self._source_pm = None
        self.clear()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self._repaint_scaled()

    def _repaint_scaled(self):
        if self._source_pm and not self._source_pm.isNull():
            scaled = self._source_pm.scaled(
                self.size() - QSize(8, 8),     # small inset margin
                Qt.AspectRatioMode.KeepAspectRatio,
                Qt.TransformationMode.SmoothTransformation,
            )
            self.setPixmap(scaled)


# ════════════════════════════════════════════════════════════════════════════
#  PROGRESS BAR HELPER  (indeterminate ↔ determinate)
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
    if indeterminate:
        pb.setRange(0, 0)
    else:
        pb.setRange(0, 100)
        pb.setValue(0)
    pb.setVisible(True)


def progress_done(pb: QProgressBar):
    pb.setRange(0, 100)
    pb.setValue(100)
    pb.setVisible(False)


# ════════════════════════════════════════════════════════════════════════════
#  DIRECTORY DATABASE PANEL
# ════════════════════════════════════════════════════════════════════════════

class DirDatabasePanel(QWidget):
    """
    Portable JSON directory store.
    Paths are stored relative to the script directory when possible.

    Signals:
        dir_loaded(str):  absolute path selected by user
    """
    dir_loaded = pyqtSignal(str)

    def __init__(self, get_current_dir, parent=None):
        super().__init__(parent)
        self._get_current_dir = get_current_dir
        self._base            = os.path.dirname(os.path.abspath(sys.argv[0]))
        self._db_path         = _db_path()
        self._db              = {"entries": []}
        self._thread          = None
        self._worker          = None
        self._build_ui()
        self._load_db()

    # ── UI ────────────────────────────────────────────────────────────────
    def _build_ui(self):
        lv = QVBoxLayout(self)
        lv.setContentsMargins(6, 6, 6, 6)
        lv.setSpacing(4)

        hdr = QLabel("📁  Directory Database")
        hdr.setStyleSheet("font-weight: bold; font-size: 11px;")
        lv.addWidget(hdr)

        self.line_name = QLineEdit()
        self.line_name.setPlaceholderText("Entry name (auto-filled on Save)…")
        lv.addWidget(self.line_name)

        self.list_widget = QListWidget()
        self.list_widget.setSelectionMode(
            QAbstractItemView.SelectionMode.SingleSelection)
        self.list_widget.setToolTip("Double-click to load from DB")
        self.list_widget.itemDoubleClicked.connect(self._on_double_click)
        lv.addWidget(self.list_widget)

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

    # ── Public ────────────────────────────────────────────────────────────
    def save_current_dir(self):
        path = self._get_current_dir()
        if not path or not os.path.isdir(path):
            self.lbl_status.setText("⚠  No valid directory active.")
            return
        # Use auto-name if field is empty
        name = self.line_name.text().strip() or auto_entry_name(path)
        self.line_name.setText(name)

        try:
            stored = os.path.relpath(path, self._base)
        except ValueError:
            stored = path   # different drive on Windows

        for entry in self._db["entries"]:
            if entry["name"] == name:
                entry["path"] = stored
                break
        else:
            self._db["entries"].append({"name": name, "path": stored})

        self._refresh_list()
        self._save_db()

    def load_selected(self):
        item = self.list_widget.currentItem()
        if item:
            self._emit_from_name(item.text())

    def delete_selected(self):
        item = self.list_widget.currentItem()
        if not item:
            return
        name = item.text()
        self._db["entries"] = [
            e for e in self._db["entries"] if e["name"] != name
        ]
        self._save_db()
        self._refresh_list()

    # ── Private ───────────────────────────────────────────────────────────
    def _on_double_click(self, item):
        self._emit_from_name(item.text())

    def _emit_from_name(self, name: str):
        for entry in self._db["entries"]:
            if entry["name"] == name:
                abs_path = os.path.normpath(
                    os.path.join(self._base, entry["path"]))
                if os.path.isdir(abs_path):
                    self.dir_loaded.emit(abs_path)
                    self.lbl_status.setText(f"✓  {abs_path}")
                else:
                    QMessageBox.warning(
                        self, "Path not found",
                        f"Could not resolve:\n{abs_path}\n\n"
                        f"(Stored: {entry['path']})")
                return

    def _refresh_list(self):
        self.list_widget.clear()
        for e in self._db["entries"]:
            self.list_widget.addItem(e["name"])

    def _save_db(self):
        progress_start(self.pb, indeterminate=True)
        self.btn_save.setEnabled(False)
        w = DBSaveWorker(self._db_path, self._db)
        self._thread, self._worker = start_threaded_worker(w, self._on_save_done)

    def _on_save_done(self, ok: bool):
        progress_done(self.pb)
        self.btn_save.setEnabled(True)
        self.lbl_status.setText("✓  Saved." if ok else "✗  Save failed.")
        cleanup_thread(self._thread, self._worker)

    def _load_db(self):
        progress_start(self.pb, indeterminate=True)
        w = DBLoadWorker(self._db_path)
        self._thread, self._worker = start_threaded_worker(w, self._on_load_done)

    def _on_load_done(self, result: tuple):
        ok, data = result
        progress_done(self.pb)
        if ok and isinstance(data, dict) and "entries" in data:
            self._db = data
        self._refresh_list()
        cleanup_thread(self._thread, self._worker)


# ════════════════════════════════════════════════════════════════════════════
#  ALEAVUE TAB  (Slideshow)
#  AleaVue = Latin alea (dice/chance) + French vue (view)
# ════════════════════════════════════════════════════════════════════════════

class AleaVueTab(QWidget):
    """Tab for the AleaVue random image slideshow."""
    request_dir_change = pyqtSignal(str)

    def __init__(self, get_global_dir, parent=None):
        super().__init__(parent)
        self._get_global_dir = get_global_dir
        self._image_paths: list[str] = []
        self._slideshow_window = None
        self._log_path = os.path.join(
            os.path.dirname(os.path.abspath(sys.argv[0])), "optegnelser")
        self._log = False
        self._scan_thread = None
        self._scan_worker = None
        self._build_ui()

    def _build_ui(self):
        outer = QHBoxLayout(self)

        # ── Left: controls ─────────────────────────────────────────────
        left = QWidget()
        lv = QVBoxLayout(left)
        lv.setSpacing(6)

        self.lbl_status = QLabel("No directory selected.")
        self.lbl_status.setWordWrap(True)
        lv.addWidget(self.lbl_status)

        # Progress bar (determinate, shown during scan)
        self.pb = make_progress_bar()
        lv.addWidget(self.pb)

        # Replaced "Instant Load" checkbox → explicit button
        self.btn_load_dir = QPushButton("📂  Load from Directory")
        self.btn_load_dir.setToolTip(
            "Scan the selected directory for images now")
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

        # ── Right: DB panel ────────────────────────────────────────────
        self._db_panel = DirDatabasePanel(self._get_global_dir)
        self._db_panel.dir_loaded.connect(self._on_db_dir_loaded)
        outer.addWidget(self._db_panel, 1)

    # ── Called by MainWindow ──────────────────────────────────────────
    def on_directory_changed(self, path: str):
        self._image_paths = []
        self.lbl_status.setText(f"Dir: {path}")

    # ── Button: Load from Directory ───────────────────────────────────
    def _load_from_directory(self):
        path = self._get_global_dir()
        if not path or not os.path.isdir(path):
            QMessageBox.warning(self, "No Directory",
                                "Please select a directory first.")
            return
        self._scan_dir(path)

    # ── DB panel callback ─────────────────────────────────────────────
    def _on_db_dir_loaded(self, path: str):
        self.request_dir_change.emit(path)
        self.lbl_status.setText(f"Loaded from DB: {path}")
        self._scan_dir(path)

    # ── Directory scan ────────────────────────────────────────────────
    def _scan_dir(self, path: str):
        progress_start(self.pb, indeterminate=False)
        self.btn_start.setEnabled(False)
        self.btn_load_dir.setEnabled(False)
        self.lbl_status.setText(f"Scanning: {path} …")
        w = ScanImageWorker(path)
        w.signal_progress.connect(self.pb.setValue)
        self._scan_thread, self._scan_worker = start_threaded_worker(
            w, self._on_scan_done)

    def _on_scan_done(self, result: tuple):
        ok, paths = result
        progress_done(self.pb)
        self.btn_start.setEnabled(True)
        self.btn_load_dir.setEnabled(True)
        cleanup_thread(self._scan_thread, self._scan_worker)
        if ok:
            self._image_paths = paths
            self.lbl_status.setText(f"✓  {len(paths)} images found.")
        else:
            self.lbl_status.setText("✗  Scan failed.")

    # ── Slideshow start ───────────────────────────────────────────────
    def _start_slideshow(self):
        directory = self._get_global_dir()
        if not directory:
            QMessageBox.warning(self, "No Directory",
                                "Please select a directory first.")
            return
        if self._image_paths:
            self._open_window(directory, self._image_paths)
        else:
            progress_start(self.pb, indeterminate=False)
            self.btn_start.setEnabled(False)
            self.btn_load_dir.setEnabled(False)
            w = ScanImageWorker(directory)
            w.signal_progress.connect(self.pb.setValue)
            self._scan_thread, self._scan_worker = start_threaded_worker(
                w, self._on_scan_then_open)

    def _on_scan_then_open(self, result: tuple):
        ok, paths = result
        progress_done(self.pb)
        self.btn_start.setEnabled(True)
        self.btn_load_dir.setEnabled(True)
        cleanup_thread(self._scan_thread, self._scan_worker)
        if not (ok and paths):
            QMessageBox.warning(self, "No Images",
                                "No image files found in the selected directory.")
            return
        self._image_paths = paths
        self._open_window(self._get_global_dir(), paths)

    def _open_window(self, directory: str, paths: list):
        self._slideshow_window = SlideshowWindow(
            directory, paths, self._log_path, self._log)
        if self.check_fullscreen.isChecked():
            self._slideshow_window.showFullScreen()
        else:
            self._slideshow_window.show()


# ════════════════════════════════════════════════════════════════════════════
#  SATTUMAPIC TAB  (Random File Opener)
# ════════════════════════════════════════════════════════════════════════════

class SattumaPicTab(QWidget):
    """Tab for the SattumaPic random file opener with scalable preview."""

    def __init__(self, get_global_dir, parent=None):
        super().__init__(parent)
        self._get_global_dir  = get_global_dir
        self._random_file     = ""
        self._all_files: list[str] = []
        self._scan_thread     = None
        self._scan_worker     = None
        self._vid_thread      = None
        self._vid_worker      = None
        self._build_ui()

    def _build_ui(self):
        outer = QHBoxLayout(self)

        # ── Left: controls ─────────────────────────────────────────────
        left = QWidget()
        left.setMaximumWidth(340)
        lv = QVBoxLayout(left)
        lv.setSpacing(6)

        row1 = QHBoxLayout()
        self.btn_random = QPushButton("🎲  Random File")
        self.btn_random.clicked.connect(self._pick_random)
        self.chk_auto_pick = QCheckBox("Auto-pick on dir select")
        row1.addWidget(self.btn_random)
        row1.addWidget(self.chk_auto_pick)
        lv.addLayout(row1)

        self.lbl_file = QLabel("No file selected.")
        self.lbl_file.setWordWrap(True)
        self.lbl_file.setStyleSheet("font-size: 10px; color: gray;")
        lv.addWidget(self.lbl_file)

        self.pb = make_progress_bar()
        lv.addWidget(self.pb)

        row2 = QHBoxLayout()
        self.btn_open_file = QPushButton("📂  Open File")
        self.btn_open_file.clicked.connect(self._open_file)
        self.btn_open_file.setEnabled(False)
        self.chk_auto_open = QCheckBox("Auto-open on random")
        row2.addWidget(self.btn_open_file)
        row2.addWidget(self.chk_auto_open)
        lv.addLayout(row2)

        self.btn_open_dir = QPushButton("📁  Open File's Directory")
        self.btn_open_dir.clicked.connect(self._open_file_dir)
        self.btn_open_dir.setEnabled(False)
        lv.addWidget(self.btn_open_dir)

        lv.addStretch()
        outer.addWidget(left)

        # ── Right: scalable preview ────────────────────────────────────
        right = QWidget()
        rv = QVBoxLayout(right)
        rv.setContentsMargins(4, 4, 4, 4)
        rv.setSpacing(4)

        hdr = QLabel("Preview")
        hdr.setAlignment(Qt.AlignmentFlag.AlignCenter)
        hdr.setStyleSheet("font-weight: bold;")
        rv.addWidget(hdr)

        self.preview_lbl = PreviewLabel()
        rv.addWidget(self.preview_lbl, 1)   # stretch = 1: fills remaining space

        self.lbl_preview_type = QLabel("")
        self.lbl_preview_type.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.lbl_preview_type.setStyleSheet("font-size: 10px; color: gray;")
        rv.addWidget(self.lbl_preview_type)

        outer.addWidget(right, 1)

    # ── Called by MainWindow ──────────────────────────────────────────
    def on_directory_changed(self, path: str):
        self._all_files = []
        if self.chk_auto_pick.isChecked():
            self._scan_and_pick(path)

    # ── Scan ──────────────────────────────────────────────────────────
    def _scan_and_pick(self, path: str):
        progress_start(self.pb, indeterminate=False)
        w = ScanAllFilesWorker(path)
        w.signal_progress.connect(self.pb.setValue)
        self._scan_thread, self._scan_worker = start_threaded_worker(
            w, self._on_scan_done)

    def _on_scan_done(self, result: tuple):
        ok, paths = result
        progress_done(self.pb)
        cleanup_thread(self._scan_thread, self._scan_worker)
        if ok:
            self._all_files = paths
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
        if self.chk_auto_open.isChecked():
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
                self.preview_lbl.set_source_pixmap(QPixmap.fromImage(img))
                self.lbl_preview_type.setText(f"Image  {ext}")
                return

        # 2) Video – try ffmpeg in background thread
        if ext in VIDEO_SUFFIXES:
            self.preview_lbl.clear_preview()
            self.lbl_preview_type.setText(f"Video  {ext}  –  extracting thumbnail…")
            if self._vid_thread:
                self._vid_thread.quit()
            w = VideoThumbnailWorker(path)
            self._vid_thread, self._vid_worker = start_threaded_worker(
                w, self._on_video_thumb)
            return

        # 3) System icon fallback (documents, archives, etc.)
        fip  = QFileIconProvider()
        icon = fip.icon(QFileInfo(path))
        if not icon.isNull():
            self.preview_lbl.set_source_pixmap(icon.pixmap(QSize(128, 128)))
            self.lbl_preview_type.setText(f"File  {ext if ext else '(no ext)'}")
        else:
            self.preview_lbl.clear_preview()
            self.lbl_preview_type.setText("No preview available")

    def _on_video_thumb(self, result: tuple):
        ok, pm = result
        if self._vid_thread:
            cleanup_thread(self._vid_thread, self._vid_worker)
            self._vid_thread = None
        ext = Path(self._random_file).suffix.lower()
        if ok and pm:
            self.preview_lbl.set_source_pixmap(pm)
            self.lbl_preview_type.setText(f"Video  {ext}  (ffmpeg)")
        else:
            # fallback to system icon
            fip  = QFileIconProvider()
            icon = fip.icon(QFileInfo(self._random_file))
            if not icon.isNull():
                self.preview_lbl.set_source_pixmap(icon.pixmap(QSize(128, 128)))
            self.lbl_preview_type.setText(
                f"Video  {ext}  "
                f"{'(icon – install ffmpeg for thumbnails)' if not ok else ''}"
            )

    # ── Open ──────────────────────────────────────────────────────────
    def _open_file(self):
        if self._random_file:
            try:
                open_path(self._random_file)
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Could not open file:\n{e}")

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
        self.setWindowTitle("TilVista  –  v0.2.0")
        self.setMinimumSize(760, 520)

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

        self._aleavue_tab.request_dir_change.connect(self._on_dir_from_db)

        self._tabs.addTab(self._aleavue_tab,
                          "🖼   AleaVue  (Slideshow)")
        self._tabs.addTab(self._sattumapic_tab,
                          "🎲   SattumaPic  (Random File)")

        mv.addWidget(self._tabs)

    def _on_dir_changed(self, path: str):
        self._aleavue_tab.on_directory_changed(path)
        self._sattumapic_tab.on_directory_changed(path)

    def _on_dir_from_db(self, path: str):
        self._dir_bar.set_directory(path)
        self._sattumapic_tab.on_directory_changed(path)


# ════════════════════════════════════════════════════════════════════════════
#  SLIDESHOW WINDOW  (AleaVue Fullscreen Player)
# ════════════════════════════════════════════════════════════════════════════

class SlideshowWindow(QMainWindow):
    def __init__(self, directory: str, image_paths: list,
                 logpath: str, log: bool):
        super().__init__()
        self.setWindowTitle("TilVista · AleaVue")
        self.setStyleSheet("background-color: black;")

        self._log_path    = logpath
        self._log         = log
        self._directory   = directory
        self._image_paths = image_paths
        self._image_order: list[str] = []
        self._backtrack   = 0
        self._current     = ""

        self.setCursor(Qt.CursorShape.BlankCursor)
        _prevent_sleep()

        screen = QApplication.primaryScreen().availableGeometry()
        self._show_w    = screen.width()
        self._show_h    = screen.height()
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
        self._show_w    = s.width()
        self._show_h    = s.height()
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
        if orig.isValid() and (
                orig.width() > self._show_w or orig.height() > self._show_h):
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