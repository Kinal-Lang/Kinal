#!/usr/bin/env python3
"""Upload a fix script to the remote server and execute it to fix Web.kn paths."""
import paramiko

HOST = "192.168.1.12"
USER = "yiyi"
PASSWORD = "12345678"

FIX_SCRIPT = r'''import sys

def fix_file(path):
    with open(path, 'r') as f:
        text = f.read()

    lines = text.split('\n')
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith('string managed_path') and 'IO.Path.Cwd()' in stripped:
            lines[i] = 'string managed_path = IO.Path.Cwd() + "/managed/";'
        elif stripped.startswith('string html_path') and 'managed_path' in stripped:
            lines[i] = 'string html_path = managed_path + "html/";'
        elif stripped.startswith('string static_path') and 'managed_path' in stripped:
            lines[i] = 'string static_path = managed_path + "static/";'
        elif stripped.startswith('string content_path') and 'static_path' in stripped:
            lines[i] = 'string content_path = static_path + "content/";'
        elif 'zh-CN' in stripped and 'index.html' in stripped and 'html_path' in stripped:
            lines[i] = '    ServePage(context, html_path + "zh-CN/index.html");'

    with open(path, 'w') as f:
        f.write('\n'.join(lines))
    print('[OK] fixed ' + path)

fix_file(sys.argv[1])
'''

client = paramiko.SSHClient()
client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
client.connect(HOST, port=22, username=USER, password=PASSWORD, timeout=10)
sftp = client.open_sftp()
with sftp.open("/tmp/fix_paths.py", "w") as f:
    f.write(FIX_SCRIPT)
sftp.close()

stdin, stdout, stderr = client.exec_command(
    "python3 /tmp/fix_paths.py /home/yiyi/桌面/Dev/kinal.org/src/Web.kn"
)
print(stdout.read().decode())
err = stderr.read().decode()
if err:
    print("STDERR:", err)
client.close()
