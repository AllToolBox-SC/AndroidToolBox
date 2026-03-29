# By XTC Easy ROOT, NOT ATB Developer
# 若有侵权，请提issue联系删除
# For LICENSE please see down_xtcpatch.py_LICENSE.txt

import hashlib
import uuid
import time
import requests
import os
import sys
import tempfile
import base64
import zipfile
from urllib.parse import urlparse
from base64 import b64decode

PRIVATE_KEY = b64decode("ZGhkaGczNmM=").decode("utf-8")
UID = 1814215835
EXPIRED_TIME_SEC = 3153600000 
path = os.path
join = os.path.join
gettempdir = tempfile.gettempdir
TIMESTAMP_FILE = join(gettempdir(), "last_run.txt")


def md5sum(unsigned_str: str) -> str:
    return hashlib.md5(unsigned_str.encode()).hexdigest()


def get_signed_url(path: str) -> str:
    timestamp = int(time.time()) + EXPIRED_TIME_SEC
    random_uuid = str(uuid.uuid4()).replace("-", "")
    unsigned_str = f"{path}-{timestamp}-{random_uuid}-{UID}-{PRIVATE_KEY}"
    # According to the bytecode, the returned string order is: ts - uuid - UID - md5(...)
    auth_key = f"{timestamp}-{random_uuid}-{UID}-{md5sum(unsigned_str)}"
    return auth_key


def check_time_interval() -> bool:
    """
    Returns False if the last run was within 3600 seconds; True otherwise.
    """
    try:
        if os.path.exists(TIMESTAMP_FILE):
            with open(TIMESTAMP_FILE, "r") as f:
                last_time = float(f.read().strip())
            if time.time() - last_time < 3600:
                print("距离上次运行不足10分钟，无需下载")
                return False
        return True
    except Exception:
        # On any error, allow proceeding
        return True


def save_timestamp():
    try:
        with open(TIMESTAMP_FILE, "w") as f:
            f.write(str(time.time()))
    except Exception:
        pass


def is_valid_zip(file_path: str) -> bool:
    try:
        with zipfile.ZipFile(file_path, "r") as zip_ref:
            return zip_ref.testzip() is None
    except Exception:
        return False


def check_and_clean_invalid_zip(file_path: str):
    if os.path.exists(file_path) and not is_valid_zip(file_path):
        os.remove(file_path)
        print(f"检测到无效的ZIP文件，已删除: {file_path}")


def download_file(
    url: str,
    download_path: str,
    show_progress: bool = True,
    show_completion: bool = True,
):
    try:
        print(url)
        response = requests.get(url, stream=True)
        response.raise_for_status()

        if show_progress:
            total_size = int(response.headers.get("content-length", 0))
        else:
            total_size = 0

        downloaded = 0
        with open(download_path, "wb") as f:
            for chunk in response.iter_content(chunk_size=8192):
                if not chunk:
                    continue
                f.write(chunk)
                if show_progress and total_size > 0:
                    downloaded += len(chunk)
                    progress = downloaded / total_size * 100
                    print(f"\r下载进度: {progress:.2f}%", end="")
        if show_completion:
            print(f"\n下载完成: {download_path}")
        return True
    except Exception as e:
        print(f"下载失败: {e}")
        return False


def get_executable_dir() -> str:
    if getattr(sys, "frozen", False):
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))


def main():
    # Require exactly one arg: "1" (notice) or "2" (downloads)
    if len(sys.argv) != 2:
        print("Usage: python down_xtcpatch.py <1|2>")
        sys.exit(1)
    arg = sys.argv[1]
    if arg not in ("1", "2"):
        print("Usage: python down_xtcpatch.py <1|2>")
        sys.exit(1)

    base_dir = get_executable_dir()
    download_dir = join(base_dir, "res", "modules")
    if not os.path.exists(download_dir):
        os.makedirs(download_dir, exist_ok=True)

    if arg == "1":
        base_url = "https://1814215835.v.123pan.cn/1814215835/xtc_root/ezr_file/ezr_notice"
        download_path = join(download_dir, "ezr_notice.txt")
        parsed_url = urlparse(base_url)
        path_part = parsed_url.path
        auth_key = get_signed_url(path_part)
        signed_url = f"{base_url}?auth_key={auth_key}"
        if download_file(signed_url, download_path, show_progress=False, show_completion=False):
            try:
                with open(download_path, "r", encoding="utf-8") as f:
                    content = f.read()
                print("[NOTICE]", end="")
                print(content)
            except Exception as e:
                print("读取文件失败: ", e)
        return

    # arg == "2"
    if not check_time_interval():
        return

    xtcpatch_path = join(download_dir, "xtcpatch.zip")
    systemui_path = join(download_dir, "systemui.zip")
    check_and_clean_invalid_zip(xtcpatch_path)
    check_and_clean_invalid_zip(systemui_path)

    save_timestamp()

    files_to_download = [
        {
            "base_url": "https://1814215835.v.123pan.cn/1814215835/xtc_root/xtcpatch/xtcpatch.zip",
            "download_path": xtcpatch_path,
        },
        {
            "base_url": "https://1814215835.v.123pan.cn/1814215835/xtc_root/xtcpatch/systemui.zip",
            "download_path": systemui_path,
        },
    ]

    for file_info in files_to_download:
        base_url = file_info["base_url"]
        download_path = file_info["download_path"]
        parsed_url = urlparse(base_url)
        path_part = parsed_url.path
        auth_key = get_signed_url(path_part)
        signed_url = f"{base_url}?auth_key={auth_key}"
        print(f"\n开始下载: {os.path.basename(download_path)}")
        if download_file(signed_url, download_path):
            check_and_clean_invalid_zip(download_path)


if __name__ == "__main__":
    main()