import os
import random
import threading
import time
from typing import Optional

import miniaudio


class AudioPlayer(object):
    def __init__(self, audio_path: str):
        self.audio_path = os.path.abspath(audio_path)
        self.audios: list[str] = []
        self.audio_device: Optional[miniaudio.PlaybackDevice] = None
        self._current_index: int = 0
        self._lock = threading.RLock()
        self._stop_event = threading.Event()
        self._worker_thread: Optional[threading.Thread] = None
        self._load_audios()

    def _load_audios(self) -> None:
        if os.path.isdir(self.audio_path):
            files = sorted(os.listdir(self.audio_path))
            self.audios = [
                os.path.join(self.audio_path, name)
                for name in files
                if os.path.isfile(os.path.join(self.audio_path, name))
            ]
        elif os.path.isfile(self.audio_path):
            self.audios = [self.audio_path]
        else:
            self.audios = []

    def _play_index(self, index: int) -> Optional[str]:
        target = self.audios[index]
        try:
            stream = miniaudio.stream_file(target)
            wrapped_stream = miniaudio.stream_with_callbacks(stream)
            next(wrapped_stream)

            device = miniaudio.PlaybackDevice()
            device.start(wrapped_stream)
            self.audio_device = device
            self._current_index = index
            return target
        except Exception:
            return None

    def _play_next_available(self, start_index: int) -> Optional[str]:
        if not self.audios:
            return None
        for offset in range(len(self.audios)):
            idx = (start_index + offset) % len(self.audios)
            played = self._play_index(idx)
            if played is not None:
                return played
        return None

    def _play_random_available(self, exclude_current: bool = False) -> Optional[str]:
        if not self.audios:
            return None
        candidate_indices = list(range(len(self.audios)))
        if exclude_current and len(candidate_indices) > 1:
            candidate_indices = [i for i in candidate_indices if i != self._current_index]
        random.shuffle(candidate_indices)
        for idx in candidate_indices:
            played = self._play_index(idx)
            if played is not None:
                return played
        return None

    def _wait_current_finish_or_stop(self) -> None:
        while not self._stop_event.is_set():
            with self._lock:
                device = self.audio_device
            if device is None:
                return
            # Stream end will clear callback_generator in miniaudio callback.
            if device.callback_generator is None:
                return
            time.sleep(0.1)

    def _loop_playlist_worker(self) -> None:
        while not self._stop_event.is_set():
            with self._lock:
                if not self.audios:
                    return
                self._stop_current_device()
                if self._play_random_available(exclude_current=False) is None:
                    return
            self._wait_current_finish_or_stop()
            if self._stop_event.is_set():
                break
        self._stop_current_device()

    def _stop_current_device(self) -> None:
        with self._lock:
            if self.audio_device is None:
                return
            try:
                self.audio_device.stop()
            finally:
                self.audio_device.close()
                self.audio_device = None

    def stop_audio(self) -> None:
        self._stop_event.set()
        self._stop_current_device()
        worker = self._worker_thread
        if worker is not None and worker.is_alive() and worker is not threading.current_thread():
            worker.join(timeout=0.3)
        self._worker_thread = None

    def play_audio(self, auto_next: bool = False):
        with self._lock:
            if not self.audios:
                return None
            self._stop_event.clear()
            if self._worker_thread is not None and self._worker_thread.is_alive():
                return self.audios[self._current_index]
            if auto_next:
                self._worker_thread = threading.Thread(
                    target=self._loop_playlist_worker,
                    name="AudioPlayerLoop",
                    daemon=True,
                )
                self._worker_thread.start()
                return self.audios[self._current_index]
            self._stop_current_device()
            return self._play_next_available(self._current_index)

    def next_audio(self):
        with self._lock:
            if not self.audios:
                return None
            self._stop_event.clear()
            self._stop_current_device()
            return self._play_random_available(exclude_current=True)

    def close(self) -> None:
        self.stop_audio()
