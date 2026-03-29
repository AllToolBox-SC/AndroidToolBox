# -*- coding: utf-8 -*-

import argparse
import subprocess
import sys
import datetime

def get_timestamp():
    return datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')


def execute_command(command):
    print(f'''[{get_timestamp()}] 执行命令: {command}''')
    result = subprocess.run(command, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,  text=True)
    print(result.stderr)
    if result.returncode != 0: return False
    return True


def main():
    parser = argparse.ArgumentParser(description='执行命令并格式化输出结果')
    parser.add_argument('command', help='要执行的命令')
    args = parser.parse_args()
    while True:
        success = execute_command(args.command)
        print(f'''[{get_timestamp()}] 执行完毕''')
        if success:
            return 0
        retry = input('执行失败，是否重试？(y/n): ').strip().lower()
        if retry != 'y':
            return 1
        

if __name__ == '__main__':
    sys.exit(main())
