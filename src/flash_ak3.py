from typing import Optional, Final, List
import zipfile
import os
import sys
import subprocess
import shutil


_AK3_MAINFILE: Final[List[str]] = ["zImage", "Image"]


class AnyKernel3(object):
    def __init__(self, zip_path, extract_to: Optional[str] = "./tmp"):
        """ AK3 Class INIT """
        self.ak3_zip_path: str = zip_path
        self.ak3_extract_to: str = extract_to
        self.mainfile: str | None = None
        self.bootimg_path: str | None = None
        self.bootimg_unpacked_path: str | None = None
    
    @staticmethod
    def _get_cwd() -> str:
        cwd: str # noqa
        if getattr(sys, 'frozen', False):
            # PyInstaller Support
            cwd = os.path.dirname(sys.executable)
        else:
            # Use __file__ to support Nuitka
            cwd = os.path.dirname(os.path.abspath(__file__))

        return cwd

    def extract_zip(self) -> str:
        with zipfile.ZipFile(self.ak3_zip_path, 'r') as zip_ref:
            # Find AnyKernel3 main file in the zip, one file only
            for mainfile in _AK3_MAINFILE: # noqa
                if mainfile in zip_ref.namelist():
                    zip_ref.extract(mainfile, self.ak3_extract_to)
                    self.mainfile = os.path.abspath(os.path.join(self.ak3_extract_to, mainfile))
                    return self.mainfile
            raise FileNotFoundError("No AnyKernel3 main file found in the zip.")
    
    def rename_mainfile(self, new_name: Optional[str] = "kernel") -> None: # void
        if not self.mainfile: raise FileNotFoundError("Load AnyKernel3 ZIP First")
        new_path = os.path.abspath(os.path.join(self.ak3_extract_to, new_name))
        os.rename(self.mainfile, new_path)
        self.mainfile = new_path
        return None # noqa

    def unpack_bootimg(self, bootimg_path: str, unpacked_path: str) -> str:
        bootimg_path = os.path.abspath(bootimg_path)
        unpacked_path = os.path.abspath(unpacked_path)

        self.bootimg_path = bootimg_path
        self.bootimg_unpacked_path = unpacked_path

        os.makedirs(unpacked_path, exist_ok=True)

        _cwd = self._get_cwd()
        os.chdir(unpacked_path)

        process = subprocess.run([os.path.join(_cwd, "magiskboot.exe"), "unpack", bootimg_path], check=False, text=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, encoding="utf-8")
        if process.returncode != 0: raise RuntimeError(f"Failed to unpack boot image: Returned non-zero code, STDERR: {process.stderr}")
        os.chdir(_cwd)
        del _cwd
        return self.bootimg_unpacked_path
    
    def patch_bootimg(self, unpacked_path: Optional[str | None] = None) -> None: # void
        if not unpacked_path: 
            if self.bootimg_unpacked_path: unpacked_path = self.bootimg_unpacked_path
            else: raise ValueError()
        if not self.mainfile: raise RuntimeError("Load AnyKernel3 ZIP First")
        kernel_path = os.path.abspath(os.path.join(unpacked_path, "kernel"))
        shutil.copy2(self.mainfile, kernel_path)
        return None
    
    def repack_bootimg(self, unpacked_path: Optional[str | None] = None, repack_to: Optional[str | None] = None) -> str:
        if not unpacked_path: 
            if self.bootimg_unpacked_path: unpacked_path = self.bootimg_unpacked_path
            else: raise ValueError()
        if not repack_to: 
            if self.bootimg_path: repack_to = self.bootimg_path
            else: raise ValueError()
        if not self.bootimg_path:
            raise FileNotFoundError("Unpack boot/init_boot image first")
        repack_to = os.path.abspath(repack_to)
        unpacked_path = os.path.abspath(unpacked_path)

        _cwd = self._get_cwd()
        
        os.chdir(unpacked_path)

        process = subprocess.run([os.path.join(_cwd, "magiskboot.exe"), "repack", self.bootimg_path, repack_to], check=False, text=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, encoding="utf-8")
        if process.returncode != 0: raise RuntimeError(f"Failed to repack boot image: Returned non-zero code, STDERR: {process.stderr}")

        os.chdir(_cwd)
        del _cwd

        return repack_to

    def clean_up(self, unpacked_path: Optional[str | None] = None) -> None: # void
        if not unpacked_path: 
            if self.bootimg_unpacked_path: unpacked_path = self.bootimg_unpacked_path
            else: raise ValueError()

        os.removedirs(unpacked_path)
        return None # noqa