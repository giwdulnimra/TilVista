# -*- coding: utf-8 -*-
"""
╔══════════════════════════════════════════════════════════════════╗
║  TilVista  –  Tilfeldig (nor.: zufällig) + Vista (span.: Blick)  ║
║  Sub-tools:   [Slideshow]                              ║
║              SattumaPic [Random File Opener]                     ║
╠══════════════════════════════════════════════════════════════════╣
║  @author: Ludwig, Armin                                          ║
║  @year:   2024-2025                                              ║
║  version: v0.1.0  (merged & refactored)                          ║
╚══════════════════════════════════════════════════════════════════╝

dependencies:
    pip install PyQt6

history:
    v0.1.0 – 2025 – merged RanChFO + RanSlidSHW into one app
                  – global directory bar (LineEdit + Browse)
                  – DirDB: JSON-based directory database, threaded I/O, relative paths
                  – RanChFO: image thumbnail preview, system-icon fallback
                  – threading: generic start_threaded_worker() utility
                  – cross-platform open_path() helper (Windows/Linux/macOS)
"""

import ctypes
import glob
import json
import os
import random
import subprocess
import sys
from pathlib import Path

from PyQt6.QtCore import (
    QFileInfo, QObject, QSize, QThread, Qt, QTimer, pyqtSignal,
)
from PyQt6.QtGui import QImage, QImageReader, QPixmap
from PyQt6.QtWidgets import (
    QFileIconProvider, QAbstractItemView, QApplication, QCheckBox, QFileDialog,
    QHBoxLayout, QLabel, QLineEdit, QListWidget, QMainWindow,
    QMessageBox, QPushButton, QProgressBar, QSizePolicy,
    QSpacerItem, QTabWidget, QVBoxLayout, QWidget,
)

# ─── Windows: sleep prevention ───────────────────────────────────────────────
ES_CONTINUOUS      = 0x80000000
ES_SYSTEM_REQUIRED = 0x00000001


def _prevent_sleep():
    try:
        ctypes.windll.kernel32.SetThreadExecutionState(
            ES_CONTINUOUS | ES_SYSTEM_REQUIRED)
    except AttributeError:
        pass  # non-Windows


def _restore_sleep():
    try:
        ctypes.windll.kernel32.SetThreadExecutionState(ES_CONTINUOUS)
    except AttributeError:
        pass


# ─── Cross-platform open ──────────────────────────────────────────────────────
def open_path(path: str):
    """Open file or directory with the OS default handler."""
    try:
        os.startfile(path)                          # Windows
    except AttributeError:
        if sys.platform == "darwin":
            subprocess.Popen(["open", path])
        else:
            subprocess.Popen(["xdg-open", path])


# ─── Threading utility ────────────────────────────────────────────────────────
def start_threaded_worker(worker: QObject, result_slot) -> tuple:
    """
    Moves *worker* onto a new QThread, connects worker.run to thread.started,
    connects worker.signal_result to *result_slot*, and starts the thread.
    Returns (thread, worker) so the caller can keep a reference.

    Inspired by the improvement note: 'Verbesserung_Threaded_Worker.txt'.
    """
    thread = QThread()
    worker.moveToThread(thread)
    thread.started.connect(worker.run)
    worker.signal_result.connect(result_slot)
    thread.start()
    return thread, worker


def cleanup_thread(thread: QThread, worker: QObject):
    """Safely shut down thread after signal has been received."""
    thread.quit()
    worker.deleteLater()
    thread.deleteLater()


# ════════════════════════════════════════════════════════════════════════════
#  WORKERS
# ════════════════════════════════════════════════════════════════════════════

IMAGE_SUFFIXES = {
    ".jpg", ".jpeg", ".png", ".bmp",
    ".tif", ".tiff", ".gif", ".webp",
}


class ScanImageWorker(QObject):
    """Recursively collects image paths inside *directory*."""
    signal_result = pyqtSignal(tuple)   # (success: bool, paths: list[str])

    def __init__(self, directory: str):
        super().__init__()
        self.directory = directory

    def run(self):
        paths = []
        try:
            for root, _, files in os.walk(self.directory):
                for f in files:
                    if Path(f).suffix.lower() in IMAGE_SUFFIXES:
                        paths.append(os.path.join(root, f))
            self.signal_result.emit((True, paths))
        except Exception as e:
            print(f"[ScanImageWorker] {e}")
            self.signal_result.emit((False, []))


class ScanAllFilesWorker(QObject):
    """Recursively collects ALL file paths (for RanChFO)."""
    signal_result = pyqtSignal(tuple)   # (success: bool, paths: list[str])

    def __init__(self, directory: str):
        super().__init__()
        self.directory = directory

    def run(self):
        paths = []
        try:
            for root, _, files in os.walk(self.directory):
                for f in files:
                    paths.append(os.path.join(root, f))
            self.signal_result.emit((True, paths))
        except Exception as e:
            print(f"[ScanAllFilesWorker] {e}")
            self.signal_result.emit((False, []))


class DBSaveWorker(QObject):
    """Writes the directory DB JSON to disk."""
    signal_result = pyqtSignal(bool)

    def __init__(self, db_path: str, data: dict):
        super().__init__()
        self.db_path = db_path
        self.data = data

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
    """Reads the directory DB JSON from disk."""
    signal_result = pyqtSignal(tuple)   # (success: bool, data: dict)

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


# ════════════════════════════════════════════════════════════════════════════
#  DIRECTORY DATABASE PANEL
# ════════════════════════════════════════════════════════════════════════════

class DirDatabasePanel(QWidget):
    """
    Side-panel that stores named directory entries in a portable JSON file.
    Paths are stored relative to the script location when possible.

    Signals:
        dir_loaded(str): emitted when user clicks 'Load from DB' –
                         carries the resolved absolute path.
    """
    dir_loaded = pyqtSignal(str)
    DB_FILENAME = "tilvista_dirs.json"

    def __init__(self, get_current_dir, parent=None):
        """
        Parameters
        ----------
        get_current_dir : callable -> str
            Returns the currently active global directory path.
        """
        super().__init__(parent)
        self._get_current_dir = get_current_dir
        self._base_path = os.path.dirname(os.path.abspath(sys.argv[0]))
        self._db_path = os.path.join(self._base_path, "data", self.DB_FILENAME)
        self._db = {"entries": []}
        self._thread = None
        self._worker = None
        self._build_ui()
        self._load_db()

    # ── UI ────────────────────────────────────────────────────────────────
    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(6, 6, 6, 6)
        layout.setSpacing(4)

        header = QLabel("📁  Directory Database")
        header.setStyleSheet("font-weight: bold; font-size: 11px;")
        layout.addWidget(header)

        self.line_name = QLineEdit()
        self.line_name.setPlaceholderText("Name for this entry…")
        layout.addWidget(self.line_name)

        self.list_widget = QListWidget()
        self.list_widget.setSelectionMode(
            QAbstractItemView.SelectionMode.SingleSelection)
        self.list_widget.setToolTip("Double-click to load a directory")
        self.list_widget.itemDoubleClicked.connect(self._on_item_double_clicked)
        layout.addWidget(self.list_widget)

        self.progress = QProgressBar()
        self.progress.setRange(0, 0)
        self.progress.setFixedHeight(6)
        self.progress.setTextVisible(False)
        self.progress.setVisible(False)
        layout.addWidget(self.progress)

        btn_row = QHBoxLayout()
        self.btn_save   = QPushButton("Save Dir")
        self.btn_load   = QPushButton("Load")
        self.btn_delete = QPushButton("Delete")
        for btn in (self.btn_save, self.btn_load, self.btn_delete):
            btn_row.addWidget(btn)
        self.btn_save.clicked.connect(self.save_current_dir)
        self.btn_load.clicked.connect(self.load_selected_dir)
        self.btn_delete.clicked.connect(self.delete_selected)
        layout.addLayout(btn_row)

        self.status_lbl = QLabel("")
        self.status_lbl.setStyleSheet("font-size: 10px; color: gray;")
        layout.addWidget(self.status_lbl)

    # ── Public ────────────────────────────────────────────────────────────
    def save_current_dir(self):
        path = self._get_current_dir()
        if not path or not os.path.isdir(path):
            self.status_lbl.setText("⚠ No valid directory active.")
            return
        name = self.line_name.text().strip() or os.path.basename(path) or path

        # Prefer relative path for portability
        try:
            stored = os.path.relpath(path, self._base_path)
        except ValueError:
            stored = path  # different drive on Windows → keep absolute

        # Update or insert
        for entry in self._db["entries"]:
            if entry["name"] == name:
                entry["path"] = stored
                break
        else:
            self._db["entries"].append({"name": name, "path": stored})

        self._refresh_list()
        self._save_db()

    def load_selected_dir(self):
        item = self.list_widget.currentItem()
        if not item:
            return
        self._emit_dir_from_entry(item.text())

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

    # ── Private helpers ───────────────────────────────────────────────────
    def _on_item_double_clicked(self, item):
        self._emit_dir_from_entry(item.text())

    def _emit_dir_from_entry(self, name: str):
        for entry in self._db["entries"]:
            if entry["name"] == name:
                abs_path = os.path.normpath(
                    os.path.join(self._base_path, entry["path"])
                )
                if os.path.isdir(abs_path):
                    self.dir_loaded.emit(abs_path)
                else:
                    QMessageBox.warning(
                        self, "Path not found",
                        f"Directory could not be resolved:\n{abs_path}\n\n"
                        f"(Stored: {entry['path']})"
                    )
                return

    def _refresh_list(self):
        self.list_widget.clear()
        for entry in self._db["entries"]:
            self.list_widget.addItem(entry["name"])

    def _save_db(self):
        self.progress.setVisible(True)
        self.btn_save.setEnabled(False)
        worker = DBSaveWorker(self._db_path, self._db)
        self._thread, self._worker = start_threaded_worker(
            worker, self._on_save_done)

    def _on_save_done(self, success: bool):
        self.progress.setVisible(False)
        self.btn_save.setEnabled(True)
        self.status_lbl.setText("✓ Saved." if success else "✗ Save failed.")
        cleanup_thread(self._thread, self._worker)

    def _load_db(self):
        self.progress.setVisible(True)
        worker = DBLoadWorker(self._db_path)
        self._thread, self._worker = start_threaded_worker(
            worker, self._on_load_done)

    def _on_load_done(self, result: tuple):
        success, data = result
        self.progress.setVisible(False)
        if success and isinstance(data, dict) and "entries" in data:
            self._db = data
        self._refresh_list()
        cleanup_thread(self._thread, self._worker)


# ════════════════════════════════════════════════════════════════════════════
#  SLIDESHOW TAB
# ════════════════════════════════════════════════════════════════════════════

class SlideshowTab(QWidget):
    """
    Tab widget that wraps SattumaBild (random image slideshow).
    The DB panel lives on the right side as a permanent sub-panel.
    """
    # Emitted when DB loads a directory so the main window can sync the dir bar
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

        # ── Left: controls ────────────────────────────────────────────────
        left = QWidget()
        lv = QVBoxLayout(left)

        self.lbl_status = QLabel("No directory selected.")
        self.lbl_status.setWordWrap(True)
        lv.addWidget(self.lbl_status)

        self.check_instaload = QCheckBox("Instant Load (scan on dir select)")
        self.check_fullscreen = QCheckBox("Fullscreen")
        self.check_fullscreen.setChecked(True)
        lv.addWidget(self.check_instaload)
        lv.addWidget(self.check_fullscreen)

        self.progress = QProgressBar()
        self.progress.setRange(0, 0)
        self.progress.setTextVisible(False)
        self.progress.setFixedHeight(6)
        self.progress.setVisible(False)
        lv.addWidget(self.progress)

        self.btn_start = QPushButton("▶  Start Slideshow")
        self.btn_start.setFixedHeight(48)
        self.btn_start.clicked.connect(self._start_slideshow)
        lv.addWidget(self.btn_start)

        lv.addStretch()
        outer.addWidget(left, 1)

        # ── Right: DB panel ───────────────────────────────────────────────
        self._db_panel = DirDatabasePanel(self._get_global_dir)
        self._db_panel.dir_loaded.connect(self._on_db_dir_loaded)
        outer.addWidget(self._db_panel, 1)

    # ── Called by MainWindow when global dir bar changes ──────────────────
    def on_directory_changed(self, path: str):
        self._image_paths = []
        self.lbl_status.setText(f"Dir: {path}")
        if self.check_instaload.isChecked():
            self._scan_dir(path)

    # ── DB panel callback ─────────────────────────────────────────────────
    def _on_db_dir_loaded(self, path: str):
        self.request_dir_change.emit(path)   # tell main window to update dir bar
        self.lbl_status.setText(f"Loaded from DB: {path}")
        self._scan_dir(path)

    # ── Directory scan ─────────────────────────────────────────────────────
    def _scan_dir(self, path: str):
        self.progress.setVisible(True)
        self.btn_start.setEnabled(False)
        self.lbl_status.setText(f"Scanning: {path} …")
        worker = ScanImageWorker(path)
        self._scan_thread, self._scan_worker = start_threaded_worker(
            worker, self._on_scan_done)

    def _on_scan_done(self, result: tuple):
        success, paths = result
        self.progress.setVisible(False)
        self.btn_start.setEnabled(True)
        cleanup_thread(self._scan_thread, self._scan_worker)
        if success:
            self._image_paths = paths
            self.lbl_status.setText(f"✓  {len(paths)} images found.")
        else:
            self.lbl_status.setText("✗  Scan failed.")

    # ── Slideshow start ────────────────────────────────────────────────────
    def _start_slideshow(self):
        directory = self._get_global_dir()
        if not directory:
            QMessageBox.warning(self, "No Directory",
                                "Please select a directory first.")
            return
        if self._image_paths:
            self._open_window(directory, self._image_paths)
        else:
            # Lazy scan then open
            self.progress.setVisible(True)
            self.btn_start.setEnabled(False)
            worker = ScanImageWorker(directory)
            self._scan_thread, self._scan_worker = start_threaded_worker(
                worker, self._on_scan_then_open)

    def _on_scan_then_open(self, result: tuple):
        success, paths = result
        self.progress.setVisible(False)
        self.btn_start.setEnabled(True)
        cleanup_thread(self._scan_thread, self._scan_worker)
        if not (success and paths):
            QMessageBox.warning(self, "No Images",
                                "No image files found in the selected directory.")
            return
        self._image_paths = paths
        directory = self._get_global_dir()
        self._open_window(directory, paths)

    def _open_window(self, directory: str, paths: list):
        self._slideshow_window = SlideshowWindow(
            directory, paths, self._log_path, self._log)
        if self.check_fullscreen.isChecked():
            self._slideshow_window.showFullScreen()
        else:
            self._slideshow_window.show()


# ════════════════════════════════════════════════════════════════════════════
#  RANCHFO TAB  (ZuFall-Picker)
# ════════════════════════════════════════════════════════════════════════════

PREVIEW_IMAGE_SUFFIXES = {
    ".jpg", ".jpeg", ".png", ".bmp", ".tif",
    ".tiff", ".gif", ".webp",
}
PREVIEW_SIZE = QSize(220, 220)


class RanChFOTab(QWidget):
    """
    Tab widget for the ZuFall-Picker:
    Randomly picks a file from the selected directory, shows a preview
    (thumbnail for images, system icon for everything else).
    """

    def __init__(self, get_global_dir, parent=None):
        super().__init__(parent)
        self._get_global_dir = get_global_dir
        self._random_file = ""
        self._all_files: list[str] = []
        self._scan_thread = None
        self._scan_worker = None
        self._build_ui()

    def _build_ui(self):
        outer = QHBoxLayout(self)

        # ── Left: controls ────────────────────────────────────────────────
        left = QWidget()
        lv = QVBoxLayout(left)

        row1 = QHBoxLayout()
        self.btn_random = QPushButton("🎲  Random File")
        self.btn_random.clicked.connect(self._pick_random)
        self.check_trigger_random = QCheckBox("Auto-pick on dir select")
        row1.addWidget(self.btn_random)
        row1.addWidget(self.check_trigger_random)
        lv.addLayout(row1)

        self.lbl_file = QLabel("No file selected.")
        self.lbl_file.setWordWrap(True)
        self.lbl_file.setStyleSheet("font-size: 10px; color: gray;")
        lv.addWidget(self.lbl_file)

        row2 = QHBoxLayout()
        self.btn_open_file = QPushButton("📂  Open File")
        self.btn_open_file.clicked.connect(self._open_file)
        self.btn_open_file.setEnabled(False)
        self.check_trigger_open = QCheckBox("Auto-open on random")
        row2.addWidget(self.btn_open_file)
        row2.addWidget(self.check_trigger_open)
        lv.addLayout(row2)

        self.btn_open_dir = QPushButton("📁  Open File's Directory")
        self.btn_open_dir.clicked.connect(self._open_file_directory)
        self.btn_open_dir.setEnabled(False)
        lv.addWidget(self.btn_open_dir)

        self.progress = QProgressBar()
        self.progress.setRange(0, 0)
        self.progress.setTextVisible(False)
        self.progress.setFixedHeight(6)
        self.progress.setVisible(False)
        lv.addWidget(self.progress)

        lv.addStretch()
        outer.addWidget(left, 1)

        # ── Right: preview ────────────────────────────────────────────────
        right = QWidget()
        rv = QVBoxLayout(right)
        rv.setAlignment(Qt.AlignmentFlag.AlignTop)

        preview_header = QLabel("Preview")
        preview_header.setAlignment(Qt.AlignmentFlag.AlignCenter)
        preview_header.setStyleSheet("font-weight: bold;")
        rv.addWidget(preview_header)

        self.preview_lbl = QLabel()
        self.preview_lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.preview_lbl.setFixedSize(240, 240)
        self.preview_lbl.setStyleSheet(
            "border: 1px solid #555; background-color: #1e1e1e; border-radius: 4px;")
        rv.addWidget(self.preview_lbl)

        self.lbl_preview_type = QLabel("")
        self.lbl_preview_type.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.lbl_preview_type.setStyleSheet("font-size: 10px; color: gray;")
        rv.addWidget(self.lbl_preview_type)

        rv.addStretch()
        outer.addWidget(right, 1)

    # ── Called by MainWindow ──────────────────────────────────────────────
    def on_directory_changed(self, path: str):
        self._all_files = []
        if self.check_trigger_random.isChecked():
            self._scan_and_pick(path)

    # ── Scanning ──────────────────────────────────────────────────────────
    def _scan_and_pick(self, path: str):
        self.progress.setVisible(True)
        worker = ScanAllFilesWorker(path)
        self._scan_thread, self._scan_worker = start_threaded_worker(
            worker, self._on_scan_done)

    def _on_scan_done(self, result: tuple):
        success, paths = result
        self.progress.setVisible(False)
        cleanup_thread(self._scan_thread, self._scan_worker)
        if success:
            self._all_files = paths
            self._pick_random()   # auto-pick after scan

    # ── File picking ──────────────────────────────────────────────────────
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
        if self.check_trigger_open.isChecked():
            self._open_file()

    # ── Preview ───────────────────────────────────────────────────────────
    def _update_preview(self, path: str):
        ext = Path(path).suffix.lower()

        if ext in PREVIEW_IMAGE_SUFFIXES:
            # --- Image thumbnail ---
            reader = QImageReader(path)
            orig = reader.size()
            if orig.isValid() and (orig.width() > 220 or orig.height() > 220):
                scale = min(220 / orig.width(), 220 / orig.height())
                reader.setScaledSize(
                    QSize(int(orig.width() * scale),
                          int(orig.height() * scale)))
            img = reader.read()
            if not img.isNull():
                pm = QPixmap.fromImage(img).scaled(
                    220, 220,
                    Qt.AspectRatioMode.KeepAspectRatio,
                    Qt.TransformationMode.SmoothTransformation,
                )
                self.preview_lbl.setPixmap(pm)
                self.lbl_preview_type.setText(f"Image  {ext}")
                return

        # --- System icon (video, document, archive, …) ---
        fip = QFileIconProvider()
        fi  = QFileInfo(path)
        icon = fip.icon(fi)
        if not icon.isNull():
            pm = icon.pixmap(QSize(64, 64))
            self.preview_lbl.setPixmap(pm)
            self.lbl_preview_type.setText(f"File  {ext if ext else '(no ext)'}")
        else:
            self.preview_lbl.clear()
            self.lbl_preview_type.setText("No preview available")

    # ── Open actions ──────────────────────────────────────────────────────
    def _open_file(self):
        if self._random_file:
            try:
                open_path(self._random_file)
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Could not open file:\n{e}")

    def _open_file_directory(self):
        if self._random_file:
            open_path(os.path.dirname(os.path.abspath(self._random_file)))


# ════════════════════════════════════════════════════════════════════════════
#  GLOBAL DIRECTORY BAR
# ════════════════════════════════════════════════════════════════════════════

class DirBar(QWidget):
    """LineEdit + Browse button.  Emits directory_changed(str) on valid path."""
    directory_changed = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 4, 0, 4)

        self.line_edit = QLineEdit()
        self.line_edit.setPlaceholderText(
            "Select or type a directory path…")
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
        self.setWindowTitle("TilVista  –  v0.1.0")
        self.setMinimumSize(720, 480)

        central = QWidget()
        self.setCentralWidget(central)
        mv = QVBoxLayout(central)
        mv.setContentsMargins(8, 8, 8, 8)
        mv.setSpacing(4)

        # ── Global directory bar ──────────────────────────────────────────
        self._dir_bar = DirBar()
        self._dir_bar.directory_changed.connect(self._on_dir_changed)
        mv.addWidget(self._dir_bar)

        # ── Tabs ──────────────────────────────────────────────────────────
        self._tabs = QTabWidget()

        self._slideshow_tab = SlideshowTab(self._dir_bar.get_directory)
        self._ranchfo_tab   = RanChFOTab(self._dir_bar.get_directory)

        # When DB panel loads a dir, propagate back to dir bar + other tab
        self._slideshow_tab.request_dir_change.connect(self._on_dir_from_db)

        self._tabs.addTab(self._slideshow_tab, "🖼   SattumaBild  (Slideshow)")
        self._tabs.addTab(self._ranchfo_tab,   "🎲   ZuFall-Picker  (RanChFO)")

        mv.addWidget(self._tabs)

    def _on_dir_changed(self, path: str):
        """Global dir bar changed → notify both tabs."""
        self._slideshow_tab.on_directory_changed(path)
        self._ranchfo_tab.on_directory_changed(path)

    def _on_dir_from_db(self, path: str):
        """DB panel in slideshow loaded a dir → sync dir bar + other tab."""
        self._dir_bar.set_directory(path)
        # Notify RanChFO tab as well (slideshow tab notified itself)
        self._ranchfo_tab.on_directory_changed(path)


# ════════════════════════════════════════════════════════════════════════════
#  SLIDESHOW WINDOW  (SattumaBild)
# ════════════════════════════════════════════════════════════════════════════

class SlideshowWindow(QMainWindow):
    """Full-screen (or windowed) image slideshow with keyboard navigation."""

    def __init__(self, directory: str, image_paths: list,
                 logpath: str, log: bool):
        super().__init__()
        self.setWindowTitle("TilVista · SattumaBild")
        self.setStyleSheet("background-color: black;")

        self._log_path   = logpath
        self._log        = log
        self._directory  = directory
        self._image_paths = image_paths
        self._image_order: list[str] = []
        self._backtrack  = 0
        self._current    = ""

        self.setCursor(Qt.CursorShape.BlankCursor)
        _prevent_sleep()

        screen = QApplication.primaryScreen().availableGeometry()
        self._show_w = screen.width()
        self._show_h = screen.height()
        self._show_ratio = self._show_w / self._show_h

        # ── Timer ──────────────────────────────────────────────────────────
        self._dt = 4500          # milliseconds per slide
        self._timer = QTimer(self)
        self._timer.timeout.connect(self.change_image)
        self._timer.setInterval(self._dt)
        self._timer.start()

        # ── Display ────────────────────────────────────────────────────────
        self._label = QLabel()
        self._label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._label.setStyleSheet("background-color: black;")

        container = QWidget()
        layout = QVBoxLayout(container)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self._label)
        self.setCentralWidget(container)

        self.change_image()

    # ── Resize ────────────────────────────────────────────────────────────
    def resizeEvent(self, event):
        s = event.size()
        self._show_w = s.width()
        self._show_h = s.height()
        self._show_ratio = self._show_w / self._show_h

    # ── Image cycling ─────────────────────────────────────────────────────
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
        orig = reader.size()
        if orig.isValid() and (
                orig.width() > self._show_w or orig.height() > self._show_h):
            scale = min(self._show_w / orig.width(),
                        self._show_h / orig.height())
            reader.setScaledSize(
                QSize(int(orig.width() * scale),
                      int(orig.height() * scale)))
        image = reader.read()
        if image.isNull():
            self._log_error(path, reader.errorString())
            self.change_image()
            return
        pm = QPixmap.fromImage(image)
        if pm.width() / pm.height() > self._show_ratio:
            pm = pm.scaledToWidth(
                self._show_w, Qt.TransformationMode.SmoothTransformation)
        else:
            pm = pm.scaledToHeight(
                self._show_h - 18, Qt.TransformationMode.SmoothTransformation)
        self._label.setPixmap(pm)
        self._timer.stop()
        self._timer.start()
        _prevent_sleep()

    def _log_error(self, path: str, error: str):
        if self._log:
            os.makedirs(self._log_path, exist_ok=True)
            with open(os.path.join(self._log_path, "loadingerrors.log"), "a") as f:
                f.write(f"Failed: {path}  |  {error}\n")

    # ── Keyboard controls ─────────────────────────────────────────────────
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
            self._backtrack += 1   # clamp at start

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