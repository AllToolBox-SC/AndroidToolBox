import requests
import os, sys
import time
import tqdm
import py7zr
import logging
import filehash


def main():
    print("AllToolBox 修复工具")
    times = 0
    while times <= 3:
        times += 1
        try:
            with open(".\\bin\\bugversion.txt", "r") as fv:
                vcf = open(".\\bin\\version.txt")
                vc = vcf.read().strip()
                vcf.close()
                channel = os.getenv("ATB_SYS_Channel", "").lower()
                if channel == "1":
                    manifest_url = f"https://atb.xgj.qzz.io/other/rel/bugup/{vc}/manifest.json"
                elif channel == "beta":
                    manifest_url = f"https://atb.xgj.qzz.io/other/beta/bugup/{vc}/manifest.json"
                else:
                    manifest_url = f"https://atb.xgj.qzz.io/other/bugup/{vc}/manifest.json"
                try:
                    webv = requests.get(
                        manifest_url,
                        headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.3'},
                        timeout=20,
                        verify=True,
                    )
                
                    webv.raise_for_status()
                # If the ssl verification fails, retry once without verification
                except requests.exceptions.SSLError:
                    print("抱歉，遇到了一个严重问题：SSL校验失败，可能是因为您的网络环境阻止了安全连接。请尝试关闭/开启VPN或代理，或者切换到其他网络环境后重试。如果问题仍然存在，请联系技术支持获取帮助。按任意键重试")
                    print("如果你强制跳过SSL校验，可能会导致严重的安全风险，下载到恶意软件，因此我们不提供跳过SSL校验的选项。若该问题仍然存在，请从xgj236/AllToolBox仓库下载离线修复包解压进行修复。")
                    os.system("pause")
                    continue

                webvc = webv.json()["latestBugUpdate"]["ver"]
                filev = int(fv.read().strip())
                if webvc > filev:
                    print("即将更新漏洞补丁...", end="", flush=True)
                    time.sleep(3)
                    print("开始", end="\n", flush=True)
                    url = webv.json()["latestBugUpdate"]["url"]
                    md5 = webv.json()["latestBugUpdate"]["md5"]
                    try:
                        response = requests.get(url, stream=True, timeout=30, verify=True)
                        response.raise_for_status()
                    except requests.exceptions.SSLError:
                        print("抱歉，遇到了一个严重问题：SSL校验失败，可能是因为您的网络环境阻止了安全连接。请尝试关闭/开启VPN或代理，或者切换到其他网络环境后重试。如果问题仍然存在，请联系技术支持获取帮助。按任意键重试")
                        print("如果你强制跳过SSL校验，可能会导致严重的安全风险，下载到恶意软件，因此我们不提供跳过SSL校验的选项。若该问题仍然存在，请从xgj236/AllToolBox仓库下载离线修复包解压进行修复。")
                        os.system("pause")
                        continue
                    size = int(response.headers.get("content-length", 0))
                    with tqdm.tqdm(total=size, unit="B", unit_scale=True, desc="bugjump.7z") as bar:
                        with open("bugjump.7z", "wb") as bj:
                            for data in response.iter_content(chunk_size=1024):
                                bj.write(data)
                                bar.update(len(data))
                    print("下载完成，开始解压...", end="", flush=True)
                    with py7zr.SevenZipFile("bugjump.7z", mode="r") as a:
                        a.extractall(path=".\\")
                    print("成功", end="\n", flush=True)
                    time.sleep(3)
                    os.system("cmd /c start 双击运行.exe")
                    break
        except Exception as e:
            logging.error(f"修复工具运行时出错: {e}")
            print(f"发生错误: {e}. 正在重试... ({times}/3)")
            time.sleep(2)
            
if __name__ == "__main__":
    main()
    sys.exit(0)