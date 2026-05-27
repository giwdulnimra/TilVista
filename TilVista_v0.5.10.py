# -*- coding: utf-8 -*-
"""
╔═══════════════════════════════════════════════════════════════════╗
║  TilVista  v0.5.10                                                ║
╠═══════════════════════════════════════════════════════════════════╣
║  Sub-tools:  AleaVue     – zufällige Bild-Slideshow               ║
║              SattumaPic  – zufälliger Datei-Öffner                ║
╠═══════════════════════════════════════════════════════════════════╣
║  history:                                                          ║
║   v0.5.10 – ReviewDB (DB2) in SattumaPic; reviewpics.txt →        ║
║             structured JSON; Random/Select from DB2;              ║
║             "Select to File's Directory" highlights file;         ║
║             config.json for default checkbox states               ║
║   v0.5.00 – Load-Dir-Button in DirBar; Enter commits dir for      ║
║             both tabs; S-key for bookmark in slideshow;           ║
║             tab labels without parens; kaivo lowercase            ║
║   v0.4.0  – catalogue files (.dshow / .catalogue); threaded I/O   ║
╚═══════════════════════════════════════════════════════════════════╝
"""

import ctypes, json, os, platform, random, shutil, subprocess, sys, tempfile
from datetime import datetime
from pathlib import Path

from PyQt6.QtCore import (
    QFileInfo, QObject, QSize, QThread, Qt, QTimer, pyqtSignal, QUrl,
)
from PyQt6.QtGui import QImageReader, QKeySequence, QPixmap
from PyQt6.QtWidgets import (
    QAbstractItemView, QApplication, QCheckBox, QFileDialog,
    QFileIconProvider, QHBoxLayout, QLabel, QLineEdit, QListWidget,
    QMainWindow, QMessageBox, QProgressBar, QPushButton, QSizePolicy,
    QSplitter, QStackedWidget, QTabWidget, QVBoxLayout, QWidget,
)

try:
    from PyQt6.QtMultimedia import QMediaPlayer, QAudioOutput
    from PyQt6.QtMultimediaWidgets import QVideoWidget
    HAS_MULTIMEDIA = True
except ImportError:
    HAS_MULTIMEDIA = False

HAS_FFMPEG = shutil.which("ffmpeg") is not None

# ── Windows sleep prevention ──────────────────────────────────────────────────
def _prevent_sleep():
    try: ctypes.windll.kernel32.SetThreadExecutionState(0x80000000 | 0x00000001)
    except AttributeError: pass

def _restore_sleep():
    try: ctypes.windll.kernel32.SetThreadExecutionState(0x80000000)
    except AttributeError: pass

# ── Cross-platform open ───────────────────────────────────────────────────────
def open_path(path: str):
    try: os.startfile(path)
    except AttributeError:
        if sys.platform == "darwin": subprocess.Popen(["open", path])
        else: subprocess.Popen(["xdg-open", path])

def select_in_explorer(path: str):
    """Open parent directory and highlight the file (platform-specific)."""
    if not os.path.exists(path):
        return
    if sys.platform == "win32":
        subprocess.Popen(["explorer", "/select,", os.path.normpath(path)])
    elif sys.platform == "darwin":
        subprocess.Popen(["open", "-R", path])
    else:
        # Best-effort on Linux: try common file managers
        parent = os.path.dirname(path)
        for cmd in [
            ["nautilus", "--select", path],
            ["dolphin", "--select", path],
            ["thunar", parent],
        ]:
            if shutil.which(cmd[0]):
                subprocess.Popen(cmd)
                return
        open_path(parent)

# ── Path helpers ──────────────────────────────────────────────────────────────
def _script_dir() -> str:
    return os.path.dirname(os.path.abspath(sys.argv[0]))

def _kaivo_dir() -> str:
    return os.path.join(_script_dir(), "optegnelser", "kaivo")

def _kaivo_db_path() -> str:
    return os.path.join(_kaivo_dir(), "tilvista_dirs.json")

def _review_db_path() -> str:
    """DB2: bookmarks collected during AleaVue slideshow."""
    return os.path.join(_kaivo_dir(), "tilvista_review.json")

def _config_path() -> str:
    return os.path.join(_script_dir(), "tilvista_config.json")

def _log_dir() -> str:
    return os.path.join(_script_dir(), "optegnelser")

def _safe_filename(name: str) -> str:
    for c in r'\/:*?"<>|':
        name = name.replace(c, "_")
    return name.strip(".") or "entry"

def auto_entry_name(path: str) -> str:
    folder = os.path.basename(os.path.normpath(path)) or path
    return f"{folder}_{datetime.now().strftime('%y%m%d')}"

# ── File-type sets ─────────────────────────────────────────────────────────────
IMAGE_SUFFIXES = {".jpg",".jpeg",".png",".bmp",".tif",".tiff",".gif",".webp"}
VIDEO_SUFFIXES = {".mp4",".mkv",".avi",".mov",".wmv",".flv",".webm",".m4v",".ts",".mts"}

# ════════════════════════════════════════════════════════════════════════════
#  CONFIG MANAGER
# ════════════════════════════════════════════════════════════════════════════

DEFAULTS = {
    "auto_pick_on_dir_select": True,    # SattumaPic: enabled by default
    "auto_open_on_random":     False,
    "fullscreen_slideshow":    True,
}

class Config:
    """Reads / writes tilvista_config.json. Changed only via file, not in GUI."""
    _data: dict = {}

    @classmethod
    def load(cls):
        try:
            with open(_config_path(), "r", encoding="utf-8") as f:
                cls._data = json.load(f)
        except Exception:
            cls._data = {}

    @classmethod
    def get(cls, key: str):
        return cls._data.get(key, DEFAULTS.get(key))

# ════════════════════════════════════════════════════════════════════════════
#  THREADING HELPERS
# ════════════════════════════════════════════════════════════════════════════

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

# ── Progress bar helpers ──────────────────────────────────────────────────────
def pb_start(pb: QProgressBar, indeterminate=False):
    if indeterminate: pb.setRange(0, 0)
    else: pb.setRange(0, 100); pb.setValue(0)
    pb.setVisible(True)

def pb_done(pb: QProgressBar):
    pb.setRange(0, 100); pb.setValue(100); pb.setVisible(False)

def make_pb(parent=None) -> QProgressBar:
    pb = QProgressBar(parent)
    pb.setFixedHeight(7); pb.setTextVisible(False); pb.setVisible(False)
    return pb

# ════════════════════════════════════════════════════════════════════════════
#  WORKERS
# ════════════════════════════════════════════════════════════════════════════

class ScanWorker(QObject):
    signal_result   = pyqtSignal(tuple)   # (ok, image_files, all_files)
    signal_progress = pyqtSignal(int)

    def __init__(self, directory: str):
        super().__init__()
        self.directory = directory

    def run(self):
        all_files = []; image_files = []
        try:
            for root, _, files in os.walk(self.directory):
                for f in files:
                    all_files.append(os.path.join(root, f))
            total = len(all_files) or 1
            for i, fp in enumerate(all_files):
                if Path(fp).suffix.lower() in IMAGE_SUFFIXES:
                    image_files.append(fp)
                self.signal_progress.emit(int((i + 1) / total * 100))
            self.signal_result.emit((True, image_files, all_files))
        except Exception as e:
            print(f"[ScanWorker] {e}")
            self.signal_result.emit((False, [], []))


class DBSaveWorker(QObject):
    signal_result = pyqtSignal(bool)
    def __init__(self, path: str, data: dict):
        super().__init__(); self.path = path; self.data = data
    def run(self):
        try:
            os.makedirs(os.path.dirname(self.path), exist_ok=True)
            with open(self.path, "w", encoding="utf-8") as f:
                json.dump(self.data, f, indent=2, ensure_ascii=False)
            self.signal_result.emit(True)
        except Exception as e:
            print(f"[DBSaveWorker] {e}"); self.signal_result.emit(False)


class DBLoadWorker(QObject):
    signal_result = pyqtSignal(tuple)
    def __init__(self, path: str):
        super().__init__(); self.path = path
    def run(self):
        try:
            with open(self.path, "r", encoding="utf-8") as f:
                self.signal_result.emit((True, json.load(f)))
        except Exception:
            self.signal_result.emit((False, {}))


class CatalogueWriteWorker(QObject):
    signal_result = pyqtSignal(bool)
    def __init__(self, kaivo_dir, db_path, db_data, entry_name,
                 image_files, all_files):
        super().__init__()
        self.kaivo_dir   = kaivo_dir
        self.db_path     = db_path
        self.db_data     = db_data
        self.entry_name  = entry_name
        self.image_files = image_files
        self.all_files   = all_files

    def run(self):
        try:
            os.makedirs(self.kaivo_dir, exist_ok=True)
            safe = _safe_filename(self.entry_name)
            img_f = f"{safe}_img.dshow"
            all_f = f"{safe}_all.catalogue"
            with open(os.path.join(self.kaivo_dir, img_f), "w", encoding="utf-8") as f:
                f.write("\n".join(self.image_files))
            with open(os.path.join(self.kaivo_dir, all_f), "w", encoding="utf-8") as f:
                f.write("\n".join(self.all_files))
            for entry in self.db_data.get("entries", []):
                if entry["name"] == self.entry_name:
                    entry["image_files"] = img_f
                    entry["all_files"]   = all_f
                    break
            with open(self.db_path, "w", encoding="utf-8") as f:
                json.dump(self.db_data, f, indent=2, ensure_ascii=False)
            self.signal_result.emit(True)
        except Exception as e:
            print(f"[CatalogueWriteWorker] {e}"); self.signal_result.emit(False)


class CatalogueLoadWorker(QObject):
    signal_result = pyqtSignal(tuple)   # (ok, image_files, all_files)
    def __init__(self, kaivo_dir, img_fname, all_fname):
        super().__init__()
        self.kaivo_dir = kaivo_dir
        self.img_fname = img_fname
        self.all_fname = all_fname

    @staticmethod
    def _read(path):
        if not os.path.isfile(path): return []
        with open(path, "r", encoding="utf-8") as f:
            return [l.strip() for l in f if l.strip()]

    def run(self):
        try:
            img = self._read(os.path.join(self.kaivo_dir, self.img_fname))
            all_ = self._read(os.path.join(self.kaivo_dir, self.all_fname))
            self.signal_result.emit((True, img, all_))
        except Exception as e:
            print(f"[CatalogueLoadWorker] {e}")
            self.signal_result.emit((False, [], []))


class VideoFramesWorker(QObject):
    signal_result = pyqtSignal(tuple)
    FRAME_TIMES = [0, 10, 20, 30, 40, 50]
    def __init__(self, path: str):
        super().__init__()
        self.path   = path
        self.tmpdir = tempfile.mkdtemp(prefix="tilvista_vid_")
    def run(self):
        if not HAS_FFMPEG:
            self.signal_result.emit((False, [])); return
        frames = []
        for t in self.FRAME_TIMES:
            out = os.path.join(self.tmpdir, f"f{t:03d}.jpg")
            try:
                r = subprocess.run(
                    ["ffmpeg", "-i", self.path, "-ss", str(t),
                     "-vframes", "1", "-f", "image2", "-y", out],
                    capture_output=True, timeout=10)
                if r.returncode == 0 and os.path.isfile(out):
                    frames.append(out)
            except Exception: pass
        self.signal_result.emit((bool(frames), frames))


# ════════════════════════════════════════════════════════════════════════════
#  SCALABLE PREVIEW LABEL
# ════════════════════════════════════════════════════════════════════════════

class PreviewLabel(QLabel):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._src: QPixmap | None = None
        self.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.setMinimumSize(60, 60)
        self.setStyleSheet("border:1px solid #555;background:#1a1a1a;border-radius:4px;")

    def set_source_pixmap(self, pm: QPixmap):
        self._src = pm; self._repaint()

    def clear_preview(self):
        self._src = None; self.clear()

    def resizeEvent(self, e):
        super().resizeEvent(e); self._repaint()

    def _repaint(self):
        if self._src and not self._src.isNull():
            self.setPixmap(self._src.scaled(
                self.size() - QSize(8, 8),
                Qt.AspectRatioMode.KeepAspectRatio,
                Qt.TransformationMode.SmoothTransformation))


# ════════════════════════════════════════════════════════════════════════════
#  VIDEO PREVIEW WIDGET
# ════════════════════════════════════════════════════════════════════════════

class VideoPreviewWidget(QStackedWidget):
    status_text = pyqtSignal(str)
    FRAME_CYCLE_MS = 10_000

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        if HAS_MULTIMEDIA:
            self._video_w = QVideoWidget()
            self._player  = QMediaPlayer()
            self._audio   = QAudioOutput()
            self._audio.setVolume(0.0)
            self._player.setAudioOutput(self._audio)
            self._player.setVideoOutput(self._video_w)
            self._player.setLoops(QMediaPlayer.Loops.Infinite)
            self.addWidget(self._video_w)
        else:
            self.addWidget(QLabel("(no multimedia)"))
        self._frame_label = PreviewLabel()
        self.addWidget(self._frame_label)
        self._cycle_timer = QTimer(self)
        self._cycle_timer.timeout.connect(self._next_frame)
        self._cycle_timer.setInterval(self.FRAME_CYCLE_MS)
        self._frame_paths: list[str] = []
        self._frame_idx = 0
        self._tmpdir: str | None = None
        self._vid_thread = None
        self._vid_worker = None
        self.setCurrentIndex(1)

    def play(self, path: str):
        self._stop()
        self._cleanup_tmp()
        if HAS_MULTIMEDIA:
            self.setCurrentIndex(0)
            self._player.setSource(QUrl.fromLocalFile(path))
            self._player.play()
            self.status_text.emit("Video  (QMediaPlayer – muted)")
        elif HAS_FFMPEG:
            self.setCurrentIndex(1)
            self.status_text.emit("Video – extracting frames…")
            w = VideoFramesWorker(path)
            self._tmpdir = w.tmpdir
            self._vid_thread, self._vid_worker = start_threaded_worker(
                w, self._on_frames_ready)
        else:
            self.setCurrentIndex(1)
            self.status_text.emit("Video (install ffmpeg or Qt Multimedia)")

    def show_pixmap(self, pm: QPixmap, label=""):
        self._stop(); self._cleanup_tmp()
        self.setCurrentIndex(1)
        self._frame_label.set_source_pixmap(pm)
        self.status_text.emit(label)

    def clear_preview(self):
        self._stop(); self._cleanup_tmp()
        self.setCurrentIndex(1)
        self._frame_label.clear_preview()
        self.status_text.emit("")

    def _stop(self):
        self._cycle_timer.stop()
        if HAS_MULTIMEDIA: self._player.stop()
        if self._vid_thread:
            self._vid_thread.quit()
            self._vid_thread = self._vid_worker = None

    def _cleanup_tmp(self):
        if self._tmpdir and os.path.isdir(self._tmpdir):
            shutil.rmtree(self._tmpdir, ignore_errors=True)
        self._tmpdir = None; self._frame_paths = []

    def _on_frames_ready(self, result: tuple):
        ok, paths = result
        if self._vid_thread:
            cleanup_thread(self._vid_thread, self._vid_worker)
            self._vid_thread = self._vid_worker = None
        if ok and paths:
            self._frame_paths = paths; self._frame_idx = 0
            self._show_frame(0); self._cycle_timer.start()
            self.status_text.emit(
                f"Video (ffmpeg, {len(paths)} frames, "
                f"{self.FRAME_CYCLE_MS//1000}s cycle)")
        else:
            self.status_text.emit("Video (frame extraction failed)")

    def _next_frame(self):
        if not self._frame_paths: return
        self._frame_idx = (self._frame_idx + 1) % len(self._frame_paths)
        self._show_frame(self._frame_idx)

    def _show_frame(self, idx: int):
        pm = QPixmap(self._frame_paths[idx])
        if not pm.isNull(): self._frame_label.set_source_pixmap(pm)


# ════════════════════════════════════════════════════════════════════════════
#  GLOBAL DIRECTORY BAR  (v0.5.00: + Load button; Enter triggers both tabs)
# ════════════════════════════════════════════════════════════════════════════

class DirBar(QWidget):
    """LineEdit | Browse | Load from Directory
    directoryChanged  – emitted when a valid path is set (browse or Enter)
    loadRequested     – emitted when Load-button is clicked
    """
    directory_changed = pyqtSignal(str)
    load_requested    = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 4, 0, 4)

        self.line_edit = QLineEdit()
        self.line_edit.setPlaceholderText("Select or type a directory path…")
        self.line_edit.returnPressed.connect(self._on_enter)   # v0.5.00

        self.btn_browse = QPushButton("Browse")
        self.btn_browse.setFixedWidth(70)
        self.btn_browse.clicked.connect(self._browse)

        self.btn_load = QPushButton("Load from Directory")  # moved from AleaVueTab
        self.btn_load.clicked.connect(self.load_requested.emit)

        layout.addWidget(self.line_edit)
        layout.addWidget(self.btn_browse)
        layout.addWidget(self.btn_load)

    def _browse(self):
        path = QFileDialog.getExistingDirectory(self, "Select Directory")
        if path:
            self.set_directory(path)
            self.directory_changed.emit(path)

    def _on_enter(self):
        """Enter in LineEdit: validate path, emit directoryChanged (for both tabs)."""
        path = self.line_edit.text().strip()
        if os.path.isdir(path):
            self.directory_changed.emit(path)

    def get_directory(self) -> str:
        return self.line_edit.text().strip()

    def set_directory(self, path: str):
        self.line_edit.setText(path)


# ════════════════════════════════════════════════════════════════════════════
#  DIRECTORY DATABASE PANEL  (v0.5.00: "kaivo" lowercase)
# ════════════════════════════════════════════════════════════════════════════

class DirDatabasePanel(QWidget):
    """Persistent Kaivo directory store (DB1)."""
    dir_loaded = pyqtSignal(str, list, list)   # (abs_path, image_files, all_files)

    def __init__(self, get_current_dir, parent=None):
        super().__init__(parent)
        self._get_dir      = get_current_dir
        self._base         = _script_dir()
        self._kaivo        = _kaivo_dir()
        self._db_path      = _kaivo_db_path()
        self._db           = {"entries": []}
        self._thread       = self._worker = None
        self._cat_thread   = self._cat_worker = None
        self._pending_cache: dict = {}
        self._pending_load_path = ""
        self._build_ui()
        self._load_db()

    def _build_ui(self):
        lv = QVBoxLayout(self)
        lv.setContentsMargins(6, 6, 6, 6); lv.setSpacing(4)

        hdr = QLabel("kaivo  –  Directory Store")    # v0.5.00: lowercase
        hdr.setStyleSheet("font-weight:bold;font-size:11px;")
        lv.addWidget(hdr)

        self.line_name = QLineEdit()
        self.line_name.setPlaceholderText("Entry name (auto-filled after scan)…")
        lv.addWidget(self.line_name)

        self.list_widget = QListWidget()
        self.list_widget.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.list_widget.setToolTip("Double-click → Load from DB")
        self.list_widget.itemDoubleClicked.connect(
            lambda i: self._emit_from_name(i.text()))
        lv.addWidget(self.list_widget)

        self.lbl_info = QLabel("")
        self.lbl_info.setStyleSheet("font-size:9px;color:#888;")
        self.lbl_info.setWordWrap(True)
        self.list_widget.currentTextChanged.connect(self._update_info)
        lv.addWidget(self.lbl_info)

        self.pb = make_pb(); lv.addWidget(self.pb)

        btn_row = QHBoxLayout()
        self.btn_save   = QPushButton("Save to DB")
        self.btn_load   = QPushButton("Load from DB")
        self.btn_delete = QPushButton("Delete from DB")
        for b in (self.btn_save, self.btn_load, self.btn_delete):
            btn_row.addWidget(b)
        self.btn_save.clicked.connect(self.save_current_dir)
        self.btn_load.clicked.connect(
            lambda: self._emit_from_name(
                self.list_widget.currentItem().text()
                if self.list_widget.currentItem() else ""))
        self.btn_delete.clicked.connect(self._delete_selected)
        lv.addLayout(btn_row)

        self.lbl_status = QLabel("")
        self.lbl_status.setStyleSheet("font-size:10px;color:gray;")
        lv.addWidget(self.lbl_status)

    # ── Public ────────────────────────────────────────────────────────────
    def update_cache(self, path: str, image_files: list, all_files: list):
        self.line_name.setText(auto_entry_name(path))
        try: stored = os.path.relpath(path, self._base)
        except ValueError: stored = path
        for entry in self._db["entries"]:
            resolved = os.path.normpath(os.path.join(self._base, entry["path"]))
            if resolved == os.path.normpath(path):
                entry["scanned_at"] = datetime.now().isoformat(timespec="seconds")
                self._write_catalogues(entry["name"], image_files, all_files)
                self._update_info(entry["name"]); return
        self._pending_cache = {"path": stored, "image_files": image_files,
                               "all_files": all_files}

    def save_current_dir(self):
        path = self._get_dir()
        if not path or not os.path.isdir(path):
            self.lbl_status.setText("⚠  No valid directory."); return
        name = self.line_name.text().strip() or auto_entry_name(path)
        self.line_name.setText(name)
        try: stored = os.path.relpath(path, self._base)
        except ValueError: stored = path
        safe = _safe_filename(name)
        img_f = f"{safe}_img.dshow"; all_f = f"{safe}_all.catalogue"
        ts = datetime.now().isoformat(timespec="seconds")
        for entry in self._db["entries"]:
            if entry["name"] == name:
                entry.update({"path": stored, "image_files": img_f,
                              "all_files": all_f, "scanned_at": ts}); break
        else:
            self._db["entries"].append({"name": name, "path": stored,
                "image_files": img_f, "all_files": all_f, "scanned_at": ts})
        pending = self._pending_cache; self._pending_cache = {}
        self._refresh_list()
        self._write_catalogues(name,
            pending.get("image_files", []), pending.get("all_files", []))

    # ── Private ───────────────────────────────────────────────────────────
    def _delete_selected(self):
        item = self.list_widget.currentItem()
        if not item: return
        name = item.text()
        for entry in self._db["entries"]:
            if entry["name"] == name:
                for k in ("image_files", "all_files"):
                    fp = os.path.join(self._kaivo, entry.get(k, ""))
                    if os.path.isfile(fp):
                        try: os.unlink(fp)
                        except OSError: pass
                break
        self._db["entries"] = [e for e in self._db["entries"]
                               if e["name"] != name]
        self._refresh_list(); self.lbl_info.clear()
        self._save_json_only()

    def _emit_from_name(self, name: str):
        if not name: return
        for entry in self._db["entries"]:
            if entry["name"] == name:
                abs_path = os.path.normpath(
                    os.path.join(self._base, entry["path"]))
                if not os.path.isdir(abs_path):
                    QMessageBox.warning(self, "Path not found",
                        f"Could not resolve:\n{abs_path}"); return
                img_f = entry.get("image_files", "")
                all_f = entry.get("all_files",   "")
                if img_f and all_f:
                    self._pending_load_path = abs_path
                    pb_start(self.pb, indeterminate=True)
                    w = CatalogueLoadWorker(self._kaivo, img_f, all_f)
                    self._cat_thread, self._cat_worker = start_threaded_worker(
                        w, self._on_catalogue_loaded)
                else:
                    self.dir_loaded.emit(abs_path, [], [])
                return

    def _on_catalogue_loaded(self, result: tuple):
        ok, img, all_ = result
        pb_done(self.pb)
        cleanup_thread(self._cat_thread, self._cat_worker)
        self._cat_thread = self._cat_worker = None
        self.lbl_status.setText(
            f"✓  {len(img)} images loaded." if ok else "⚠  Read error – will rescan.")
        self.dir_loaded.emit(self._pending_load_path, img, all_)

    def _update_info(self, name: str):
        for entry in self._db["entries"]:
            if entry["name"] == name:
                self.lbl_info.setText(
                    f"🖼 {entry.get('image_files','—')}\n"
                    f"📄 {entry.get('all_files','—')}\n"
                    f"⏱ {entry.get('scanned_at','—')}"); return
        self.lbl_info.clear()

    def _refresh_list(self):
        cur = self.list_widget.currentItem()
        cur_text = cur.text() if cur else ""
        self.list_widget.clear()
        for e in self._db["entries"]: self.list_widget.addItem(e["name"])
        hits = self.list_widget.findItems(cur_text, Qt.MatchFlag.MatchExactly)
        if hits: self.list_widget.setCurrentItem(hits[0])

    def _write_catalogues(self, name, image_files, all_files):
        pb_start(self.pb, indeterminate=True); self.btn_save.setEnabled(False)
        w = CatalogueWriteWorker(self._kaivo, self._db_path, self._db,
                                 name, image_files, all_files)
        self._thread, self._worker = start_threaded_worker(w, self._on_save_done)

    def _save_json_only(self):
        pb_start(self.pb, indeterminate=True)
        w = DBSaveWorker(self._db_path, self._db)
        self._thread, self._worker = start_threaded_worker(w, self._on_save_done)

    def _on_save_done(self, ok: bool):
        pb_done(self.pb); self.btn_save.setEnabled(True)
        self.lbl_status.setText("✓  Saved." if ok else "✗  Save failed.")
        cleanup_thread(self._thread, self._worker)

    def _load_db(self):
        pb_start(self.pb, indeterminate=True)
        w = DBLoadWorker(self._db_path)
        self._thread, self._worker = start_threaded_worker(w, self._on_load_done)

    def _on_load_done(self, result: tuple):
        ok, data = result; pb_done(self.pb)
        if ok and isinstance(data, dict) and "entries" in data:
            self._db = data
        self._refresh_list()
        cleanup_thread(self._thread, self._worker)


# ════════════════════════════════════════════════════════════════════════════
#  REVIEW DB PANEL  (DB2, v0.5.10)
#  Stores individual files bookmarked with 'S' during AleaVue slideshow.
#  JSON structure:
#    { "sessions": [
#        { "name": "Brasilien_250521",
#          "source_dir": "E:/...",
#          "files": ["path1", "path2", ...],
#          "created_at": "..." }
#    ]}
# ════════════════════════════════════════════════════════════════════════════

class ReviewDBPanel(QWidget):
    """DB2: panel for browsing files bookmarked during slideshow."""
    file_selected = pyqtSignal(str)    # emitted when a file is chosen to preview

    def __init__(self, parent=None):
        super().__init__(parent)
        self._db_path = _review_db_path()
        self._db      = {"sessions": []}
        self._thread  = self._worker = None
        self._build_ui()
        self._load_db()

    def _build_ui(self):
        lv = QVBoxLayout(self)
        lv.setContentsMargins(6, 6, 6, 6); lv.setSpacing(4)

        hdr = QLabel("kaivo  –  review")
        hdr.setStyleSheet("font-weight:bold;font-size:11px;")
        lv.addWidget(hdr)

        # Session selector
        self.session_list = QListWidget()
        self.session_list.setMaximumHeight(90)
        self.session_list.setSelectionMode(
            QAbstractItemView.SelectionMode.SingleSelection)
        self.session_list.currentTextChanged.connect(self._on_session_changed)
        lv.addWidget(self.session_list)

        # File list within session
        self.file_list = QListWidget()
        self.file_list.setSelectionMode(
            QAbstractItemView.SelectionMode.SingleSelection)
        self.file_list.itemDoubleClicked.connect(
            lambda i: self.file_selected.emit(i.data(Qt.ItemDataRole.UserRole)))
        lv.addWidget(self.file_list)

        self.pb = make_pb(); lv.addWidget(self.pb)

        btn_row = QHBoxLayout()
        self.btn_random = QPushButton("Random from DB")
        self.btn_select = QPushButton("Select from DB")
        btn_row.addWidget(self.btn_random)
        btn_row.addWidget(self.btn_select)
        self.btn_random.clicked.connect(self._pick_random)
        self.btn_select.clicked.connect(self._pick_selected)
        lv.addLayout(btn_row)

        self.lbl_status = QLabel("")
        self.lbl_status.setStyleSheet("font-size:10px;color:gray;")
        lv.addWidget(self.lbl_status)

    # ── Public: called by SlideshowWindow when S is pressed ───────────────
    def add_bookmark(self, file_path: str, source_dir: str):
        """Add a file to the current session (keyed by source_dir + date)."""
        session_name = (
            os.path.basename(os.path.normpath(source_dir)) +
            "_" + datetime.now().strftime("%y%m%d"))
        for s in self._db["sessions"]:
            if s["name"] == session_name:
                if file_path not in s["files"]:
                    s["files"].append(file_path)
                self._save_db(); self._refresh_sessions(); return
        self._db["sessions"].append({
            "name":       session_name,
            "source_dir": source_dir,
            "files":      [file_path],
            "created_at": datetime.now().isoformat(timespec="seconds"),
        })
        self._save_db(); self._refresh_sessions()

    # ── Private ───────────────────────────────────────────────────────────
    def _current_files(self) -> list[str]:
        item = self.session_list.currentItem()
        if not item: return []
        name = item.text()
        for s in self._db["sessions"]:
            if s["name"] == name: return s.get("files", [])
        return []

    def _on_session_changed(self, name: str):
        self.file_list.clear()
        for s in self._db["sessions"]:
            if s["name"] == name:
                for fp in s.get("files", []):
                    item = __import__("PyQt6.QtWidgets", fromlist=["QListWidgetItem"]).QListWidgetItem(
                        os.path.basename(fp))
                    item.setData(Qt.ItemDataRole.UserRole, fp)
                    self.file_list.addItem(item)
                break

    def _pick_random(self):
        files = self._current_files()
        if not files: self.lbl_status.setText("⚠  No session selected."); return
        self.file_selected.emit(random.choice(files))

    def _pick_selected(self):
        item = self.file_list.currentItem()
        if item: self.file_selected.emit(item.data(Qt.ItemDataRole.UserRole))

    def _refresh_sessions(self):
        cur = self.session_list.currentItem()
        cur_text = cur.text() if cur else ""
        self.session_list.clear()
        for s in self._db["sessions"]:
            self.session_list.addItem(s["name"])
        hits = self.session_list.findItems(cur_text, Qt.MatchFlag.MatchExactly)
        if hits: self.session_list.setCurrentItem(hits[0])

    def _save_db(self):
        w = DBSaveWorker(self._db_path, self._db)
        self._thread, self._worker = start_threaded_worker(w, self._on_save_done)

    def _on_save_done(self, ok: bool):
        self.lbl_status.setText("✓  Saved." if ok else "✗  Save failed.")
        cleanup_thread(self._thread, self._worker)

    def _load_db(self):
        pb_start(self.pb, indeterminate=True)
        w = DBLoadWorker(self._db_path)
        self._thread, self._worker = start_threaded_worker(w, self._on_load_done)

    def _on_load_done(self, result: tuple):
        ok, data = result; pb_done(self.pb)
        if ok and isinstance(data, dict) and "sessions" in data:
            self._db = data
        else:
            # Migration: import old reviewpics.txt if it exists
            self._migrate_old_log()
        self._refresh_sessions()
        cleanup_thread(self._thread, self._worker)

    def _migrate_old_log(self):
        """Import legacy reviewpics.txt into the new DB structure."""
        old = os.path.join(_log_dir(), "reviewpics.txt")
        if not os.path.isfile(old): return
        with open(old, "r", encoding="utf-8") as f:
            paths = [l.strip() for l in f if l.strip()]
        if not paths: return
        self._db["sessions"].append({
            "name":       f"legacy_{datetime.now().strftime('%y%m%d')}",
            "source_dir": "",
            "files":      paths,
            "created_at": datetime.now().isoformat(timespec="seconds"),
        })
        self._save_db()
        # Rename old file so it is not imported again
        try: os.rename(old, old + ".migrated")
        except OSError: pass


# ════════════════════════════════════════════════════════════════════════════
#  ALEAVUE TAB  (v0.5.00: no Load button; receives load_requested from DirBar)
# ════════════════════════════════════════════════════════════════════════════

class AleaVueTab(QWidget):
    request_dir_change = pyqtSignal(str)

    def __init__(self, get_global_dir, review_db_panel: ReviewDBPanel,
                 parent=None):
        super().__init__(parent)
        self._get_global_dir = get_global_dir
        self._review_db      = review_db_panel   # shared reference for bookmarks
        self._image_paths: list[str] = []
        self._all_files:   list[str] = []
        self._last_scanned_path = ""
        self._slideshow_window = None
        self._scan_thread = self._scan_worker = None
        self._build_ui()

    def _build_ui(self):
        outer = QHBoxLayout(self)

        left = QWidget()
        lv   = QVBoxLayout(left); lv.setSpacing(6)

        self.lbl_status = QLabel("No directory selected.")
        self.lbl_status.setWordWrap(True)
        lv.addWidget(self.lbl_status)

        self.pb = make_pb(); lv.addWidget(self.pb)

        self.check_fullscreen = QCheckBox("Fullscreen")
        self.check_fullscreen.setChecked(Config.get("fullscreen_slideshow"))
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

    # ── Called by DirBar's Load button ───────────────────────────────────
    def load_from_directory(self):
        path = self._get_global_dir()
        if not path or not os.path.isdir(path):
            QMessageBox.warning(self, "No Directory",
                                "Please select a directory first."); return
        self._scan_dir(path)

    # ── Called by MainWindow when global dir changes ──────────────────────
    def on_directory_changed(self, path: str):
        self._image_paths = []; self._all_files = []
        self.lbl_status.setText(f"Dir set: {path}")

    def db_panel(self) -> DirDatabasePanel:
        return self._db_panel

    # ── DB callback ───────────────────────────────────────────────────────
    def _on_db_dir_loaded(self, path, image_files, all_files):
        self.request_dir_change.emit(path)
        if image_files:
            self._image_paths = image_files; self._all_files = all_files
            self._last_scanned_path = path
            self.lbl_status.setText(f"✓  {len(image_files)} images (cache)  –  {path}")
        else:
            self.lbl_status.setText(f"Loaded from DB: {path}  – scanning…")
            self._scan_dir(path)

    # ── Scan ──────────────────────────────────────────────────────────────
    def _scan_dir(self, path: str):
        pb_start(self.pb); self.btn_start.setEnabled(False)
        self.lbl_status.setText(f"Scanning: {path} …")
        self._last_scanned_path = path
        w = ScanWorker(path)
        w.signal_progress.connect(self.pb.setValue)
        self._scan_thread, self._scan_worker = start_threaded_worker(
            w, self._on_scan_done)

    def _on_scan_done(self, result: tuple):
        ok, img, all_ = result; pb_done(self.pb); self.btn_start.setEnabled(True)
        cleanup_thread(self._scan_thread, self._scan_worker)
        if ok:
            self._image_paths = img; self._all_files = all_
            self.lbl_status.setText(f"✓  {len(img)} images found.")
            self._db_panel.update_cache(self._last_scanned_path, img, all_)

    # ── Slideshow ─────────────────────────────────────────────────────────
    def _start_slideshow(self):
        directory = self._get_global_dir()
        if not directory:
            QMessageBox.warning(self, "No Directory",
                                "Please select a directory first."); return
        if self._image_paths:
            self._open_window(directory, self._image_paths)
        else:
            pb_start(self.pb); self.btn_start.setEnabled(False)
            w = ScanWorker(directory)
            w.signal_progress.connect(self.pb.setValue)
            self._scan_thread, self._scan_worker = start_threaded_worker(
                w, self._on_scan_then_open)

    def _on_scan_then_open(self, result: tuple):
        ok, img, all_ = result; pb_done(self.pb); self.btn_start.setEnabled(True)
        cleanup_thread(self._scan_thread, self._scan_worker)
        if not (ok and img):
            QMessageBox.warning(self, "No Images",
                                "No image files found."); return
        self._image_paths = img; self._all_files = all_
        self._db_panel.update_cache(self._get_global_dir(), img, all_)
        self._open_window(self._get_global_dir(), img)

    def _open_window(self, directory, paths):
        self._slideshow_window = SlideshowWindow(
            directory, paths, _log_dir(), False, self._review_db)
        if self.check_fullscreen.isChecked():
            self._slideshow_window.showFullScreen()
        else:
            self._slideshow_window.show()


# ════════════════════════════════════════════════════════════════════════════
#  SATTUMAPIC TAB  (v0.5.10: ReviewDBPanel below file controls)
# ════════════════════════════════════════════════════════════════════════════

class SattumaPicTab(QWidget):

    def __init__(self, get_global_dir, review_db_panel: ReviewDBPanel,
                 parent=None):
        super().__init__(parent)
        self._get_global_dir = get_global_dir
        self._review_db      = review_db_panel
        self._random_file    = ""
        self._all_files: list[str] = []
        self._scan_thread = self._scan_worker = None
        self._build_ui()

    def _build_ui(self):
        outer = QHBoxLayout(self)

        # ── Left column ───────────────────────────────────────────────────
        left = QWidget(); left.setMaximumWidth(380)
        lv   = QVBoxLayout(left); lv.setSpacing(6)

        row1 = QHBoxLayout()
        self.btn_random   = QPushButton("🎲  Random File")
        # v0.5.10: auto-pick default from config
        self.chk_autopick = QCheckBox("Auto-pick on dir select")
        self.chk_autopick.setChecked(Config.get("auto_pick_on_dir_select"))
        # checkbox is driven by config, but user can still toggle for this session
        self.btn_random.clicked.connect(self._pick_random)
        row1.addWidget(self.btn_random); row1.addWidget(self.chk_autopick)
        lv.addLayout(row1)

        self.lbl_file = QLabel("No file selected.")
        self.lbl_file.setWordWrap(True)
        self.lbl_file.setStyleSheet("font-size:10px;color:gray;")
        lv.addWidget(self.lbl_file)

        self.pb = make_pb(); lv.addWidget(self.pb)

        row2 = QHBoxLayout()
        self.btn_open_file = QPushButton("📂  Open File")
        self.chk_autoopen  = QCheckBox("Auto-open on random")
        self.chk_autoopen.setChecked(Config.get("auto_open_on_random"))
        self.btn_open_file.setEnabled(False)
        self.btn_open_file.clicked.connect(self._open_file)
        row2.addWidget(self.btn_open_file); row2.addWidget(self.chk_autoopen)
        lv.addLayout(row2)

        # v0.5.10: renamed + highlights file in explorer
        self.btn_open_dir = QPushButton("📁  Select in File's Directory")
        self.btn_open_dir.setEnabled(False)
        self.btn_open_dir.clicked.connect(self._select_in_dir)
        lv.addWidget(self.btn_open_dir)

        # ── DB2 (ReviewDB) below the file controls ─────────────────────
        lv.addWidget(self._review_db)

        lv.addStretch()
        outer.addWidget(left)

        # ── Right: preview ────────────────────────────────────────────────
        right = QWidget()
        rv = QVBoxLayout(right); rv.setContentsMargins(4,4,4,4); rv.setSpacing(4)
        hdr = QLabel("Preview"); hdr.setAlignment(Qt.AlignmentFlag.AlignCenter)
        hdr.setStyleSheet("font-weight:bold;"); rv.addWidget(hdr)
        self.preview = VideoPreviewWidget(); rv.addWidget(self.preview, 1)
        self.lbl_type = QLabel("")
        self.lbl_type.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.lbl_type.setStyleSheet("font-size:10px;color:gray;")
        self.preview.status_text.connect(self.lbl_type.setText)
        rv.addWidget(self.lbl_type)
        outer.addWidget(right, 1)

        # DB2 file selected → show in preview
        self._review_db.file_selected.connect(self._preview_from_db2)

    # ── Called by MainWindow ──────────────────────────────────────────────
    def on_directory_changed(self, path: str, all_files: list | None = None):
        self._all_files = all_files or []
        if self.chk_autopick.isChecked():
            if self._all_files: self._pick_random()
            else: self._scan_and_pick(path)

    # ── Scan ──────────────────────────────────────────────────────────────
    def _scan_and_pick(self, path: str):
        pb_start(self.pb)
        w = ScanWorker(path)
        w.signal_progress.connect(self.pb.setValue)
        self._scan_thread, self._scan_worker = start_threaded_worker(
            w, self._on_scan_done)

    def _on_scan_done(self, result: tuple):
        ok, _, all_ = result; pb_done(self.pb)
        cleanup_thread(self._scan_thread, self._scan_worker)
        if ok: self._all_files = all_; self._pick_random()

    # ── Pick random ───────────────────────────────────────────────────────
    def _pick_random(self):
        directory = self._get_global_dir()
        if not directory:
            QMessageBox.warning(self, "No Directory",
                                "Please select a directory first."); return
        if not self._all_files: self._scan_and_pick(directory); return
        self._random_file = random.choice(self._all_files)
        self._display_file(self._random_file)
        if self.chk_autoopen.isChecked(): self._open_file()

    def _display_file(self, path: str):
        self.lbl_file.setText(path)
        self.btn_open_file.setEnabled(True)
        self.btn_open_dir.setEnabled(True)
        self._update_preview(path)

    # ── DB2 preview ───────────────────────────────────────────────────────
    def _preview_from_db2(self, path: str):
        self._random_file = path
        self._display_file(path)

    # ── Preview ───────────────────────────────────────────────────────────
    def _update_preview(self, path: str):
        ext = Path(path).suffix.lower()
        if ext in IMAGE_SUFFIXES:
            reader = QImageReader(path)
            orig = reader.size()
            if orig.isValid() and (orig.width() > 1024 or orig.height() > 1024):
                scale = min(1024/orig.width(), 1024/orig.height())
                reader.setScaledSize(QSize(int(orig.width()*scale),
                                           int(orig.height()*scale)))
            img = reader.read()
            if not img.isNull():
                self.preview.show_pixmap(QPixmap.fromImage(img),
                                         f"Image  {ext}"); return
        if ext in VIDEO_SUFFIXES:
            self.preview.play(path); return
        fip = QFileIconProvider()
        icon = fip.icon(QFileInfo(path))
        if not icon.isNull():
            self.preview.show_pixmap(icon.pixmap(QSize(128, 128)),
                                     f"File  {ext or '(no ext)'}") 
        else:
            self.preview.clear_preview()

    # ── Open ──────────────────────────────────────────────────────────────
    def _open_file(self):
        if self._random_file:
            try: open_path(self._random_file)
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Could not open:\n{e}")

    def _select_in_dir(self):
        """v0.5.10: open folder AND highlight the file."""
        if self._random_file:
            select_in_explorer(self._random_file)


# ════════════════════════════════════════════════════════════════════════════
#  SLIDESHOW WINDOW  (v0.5.00: S = bookmark, Enter = nothing / or future use)
# ════════════════════════════════════════════════════════════════════════════

class SlideshowWindow(QMainWindow):
    def __init__(self, directory, image_paths, logpath, log,
                 review_db: ReviewDBPanel):
        super().__init__()
        self.setWindowTitle("TilVista · AleaVue")
        self.setStyleSheet("background-color: black;")
        self._directory   = directory
        self._image_paths = image_paths
        self._logpath     = logpath
        self._log         = log
        self._review_db   = review_db
        self._image_order: list[str] = []
        self._backtrack   = 0
        self._current     = ""
        self.setCursor(Qt.CursorShape.BlankCursor)
        _prevent_sleep()

        screen = QApplication.primaryScreen().availableGeometry()
        self._show_w = screen.width()
        self._show_h = screen.height()
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
        layout    = QVBoxLayout(container)
        layout.setContentsMargins(0,0,0,0)
        layout.addWidget(self._label)
        self.setCentralWidget(container)
        self.change_image()

    def resizeEvent(self, e):
        s = e.size()
        self._show_w  = s.width(); self._show_h = s.height()
        self._show_ratio = self._show_w / self._show_h

    def change_image(self):
        if not self._image_paths: return
        if self._backtrack >= -1:
            self._backtrack = 0
            img = random.choice(self._image_paths)
            self._image_order.append(img)
            self._display_image(img)
        else:
            self._backtrack += 1
            try: self._display_image(self._image_order[self._backtrack])
            except IndexError: self._backtrack = 0; self.change_image()

    def _display_image(self, path: str):
        self._current = path
        reader = QImageReader(path)
        orig   = reader.size()
        if orig.isValid() and (orig.width()>self._show_w or orig.height()>self._show_h):
            scale = min(self._show_w/orig.width(), self._show_h/orig.height())
            reader.setScaledSize(QSize(int(orig.width()*scale), int(orig.height()*scale)))
        image = reader.read()
        if image.isNull():
            if self._log:
                os.makedirs(self._logpath, exist_ok=True)
                with open(os.path.join(self._logpath, "loadingerrors.log"), "a") as f:
                    f.write(f"Failed: {path}  |  {reader.errorString()}\n")
            self.change_image(); return
        pm = QPixmap.fromImage(image)
        if pm.width()/pm.height() > self._show_ratio:
            pm = pm.scaledToWidth(self._show_w, Qt.TransformationMode.SmoothTransformation)
        else:
            pm = pm.scaledToHeight(self._show_h-18, Qt.TransformationMode.SmoothTransformation)
        self._label.setPixmap(pm)
        self._timer.stop(); self._timer.start(self._dt)
        _prevent_sleep()

    def keyPressEvent(self, event):
        key = event.key()
        if key == Qt.Key.Key_Escape:
            self.setCursor(Qt.CursorShape.ArrowCursor)
            _restore_sleep(); self.close()
        elif key == Qt.Key.Key_Right: self.change_image()
        elif key == Qt.Key.Key_Left:  self._go_back()
        elif key == Qt.Key.Key_Space:
            if self._timer.isActive(): self._timer.stop()
            else: self._timer.start(self._dt)
        elif key == Qt.Key.Key_S:
            # v0.5.00: S = bookmark (was Enter)
            self._save_bookmark(self._current)
        elif key == Qt.Key.Key_Return:
            # Enter now does nothing in slideshow (reserved for future use)
            pass
        else:
            super().keyPressEvent(event)

    def _go_back(self):
        if not self._image_paths: return
        self._backtrack -= 1
        try: self._display_image(self._image_order[self._backtrack])
        except IndexError: self._backtrack += 1

    def _save_bookmark(self, path: str):
        """v0.5.10: add to ReviewDB (DB2) instead of flat txt file."""
        if not path: return
        self._review_db.add_bookmark(path, self._directory)


# ════════════════════════════════════════════════════════════════════════════
#  MAIN WINDOW
# ════════════════════════════════════════════════════════════════════════════

class TilVistaApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("TilVista  –  v0.5.10")
        self.setMinimumSize(820, 560)

        central = QWidget()
        self.setCentralWidget(central)
        mv = QVBoxLayout(central)
        mv.setContentsMargins(8, 8, 8, 8)
        mv.setSpacing(4)

        # ── Global dir bar (v0.5.00: now includes Load button) ───────────
        self._dir_bar = DirBar()
        self._dir_bar.directory_changed.connect(self._on_dir_changed)
        mv.addWidget(self._dir_bar)

        # ── Shared ReviewDB panel (DB2) ───────────────────────────────────
        self._review_db = ReviewDBPanel()

        # ── Tabs ──────────────────────────────────────────────────────────
        self._tabs = QTabWidget()

        self._aleavue_tab    = AleaVueTab(
            self._dir_bar.get_directory, self._review_db)
        self._sattumapic_tab = SattumaPicTab(
            self._dir_bar.get_directory, self._review_db)

        # Load button in DirBar → triggers AleaVue scan
        self._dir_bar.load_requested.connect(
            self._aleavue_tab.load_from_directory)

        # DB1 panel propagates dir to bar + SattumaPic
        self._aleavue_tab.request_dir_change.connect(self._on_dir_from_db)
        self._aleavue_tab.db_panel().dir_loaded.connect(
            self._on_db1_files_loaded)

        # v0.5.00: tab labels without parenthetical descriptions
        self._tabs.addTab(self._aleavue_tab,    "🖼  AleaVue")
        self._tabs.addTab(self._sattumapic_tab, "🎲  SattumaPic")
        mv.addWidget(self._tabs)

    def _on_dir_changed(self, path: str):
        self._aleavue_tab.on_directory_changed(path)
        self._sattumapic_tab.on_directory_changed(path)

    def _on_dir_from_db(self, path: str):
        self._dir_bar.set_directory(path)

    def _on_db1_files_loaded(self, path: str, img: list, all_: list):
        """Pass cached all_files to SattumaPic to avoid redundant scan."""
        self._sattumapic_tab.on_directory_changed(path, all_)


# ════════════════════════════════════════════════════════════════════════════
#  ENTRY POINT
# ════════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    Config.load()
    app = QApplication(sys.argv)
    window = TilVistaApp()
    window.show()
    sys.exit(app.exec())
