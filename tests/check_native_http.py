"""End-to-end checks for a probe built from tests/fixtures/native_http."""
import argparse
import contextlib
import http.client
import json
import os
from pathlib import Path
import shutil
import socket
import ssl
import subprocess
import tempfile
import time

def probe_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]

@contextlib.contextmanager
def identity(temp):
    cert, key = temp / "localhost.cer", temp / "localhost.key"
    if os.name == "nt":
        env = dict(os.environ, KINAL_TEST_CERT=str(cert))
        env.pop("PSModulePath", None)
        def ps(script, **kwargs):
            return subprocess.run([shutil.which("pwsh") or "powershell", "-NoProfile", "-NonInteractive", "-Command",
                                   "$ErrorActionPreference='Stop'\n" + script], check=True, env=env, **kwargs)
        command = r"""$c=New-SelfSignedCertificate -DnsName localhost -CertStoreLocation Cert:\CurrentUser\My -NotAfter (Get-Date).AddDays(1)
Export-Certificate -Cert $c -FilePath $env:KINAL_TEST_CERT | Out-Null
$c.Thumbprint"""
        thumb = ps(command, stdout=subprocess.PIPE, text=True).stdout.strip()
        assert len(thumb) == 40 and all(c in "0123456789ABCDEF" for c in thumb)
        try:
            yield [thumb, "CurrentUser"], ssl.DER_cert_to_PEM_cert(cert.read_bytes())
        finally:
            ps("Remove-Item -LiteralPath Cert:\\CurrentUser\\My\\" + thumb + " -DeleteKey")
    else:
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                        "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost",
                        "-keyout", key, "-out", cert], check=True, capture_output=True)
        yield [str(cert), str(key)], cert.read_text()

@contextlib.contextmanager
def server(binary, port, tls=()):
    proc = subprocess.Popen([binary, "server", str(port), *tls], stdin=subprocess.PIPE)
    try:
        for _ in range(200):
            if proc.poll() is not None:
                raise RuntimeError(f"Server exited during startup ({proc.returncode})")
            try:
                with socket.create_connection(("127.0.0.1", port), timeout=1):
                    break
            except OSError:
                time.sleep(0.05)
        else:
            raise TimeoutError("Server did not start")
        yield
    finally:
        if proc.poll() is None:
            try:
                proc.communicate(b"\n", timeout=20)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
                raise
        if proc.returncode:
            raise RuntimeError(f"Server exit {proc.returncode}")

def client(binary, mode, url, *, env=None, expect=0):
    proc = subprocess.run([binary, mode, url], capture_output=True, text=True, env=env, timeout=25)
    assert proc.returncode == expect, (proc.returncode, proc.stdout, proc.stderr)
    return proc.stdout

def run(binary):
    port = probe_port()
    with server(binary, port):
        base = f"http://127.0.0.1:{port}"
        text = client(binary, "post", base + "/echo?tag=from+client")
        assert "200\n" in text and "from client|native|hello" in text, text
        client(binary, "limit", base + "/__kinal/debug/server", expect=2)
        conn = http.client.HTTPConnection("127.0.0.1", port)
        conn.request("POST", "/echo?tag=chunked", iter([b"one", b"two"]), encode_chunked=True)
        reply = conn.getresponse()
        assert reply.status == 200 and reply.read() == b"chunked||onetwo"
        conn.close()
        for chunked in (False,True):
            conn = http.client.HTTPConnection("127.0.0.1", port)
            body = iter([b"x" * 64]) if chunked else b"x" * 64
            conn.request("POST", "/echo", body, encode_chunked=chunked)
            reply = conn.getresponse()
            assert reply.status == 413 and b"32 bytes" in reply.read()
            conn.close()
    with tempfile.TemporaryDirectory() as tmp, identity(Path(tmp)) as (args, pem):
        with server(binary, port, args):
            context = ssl.create_default_context(cadata=pem)
            conn = http.client.HTTPSConnection("localhost", port, context=context)
            conn.request("GET", "/__kinal/debug/server")
            reply = conn.getresponse()
            assert reply.status == 200 and json.loads(reply.read())["backend"] == "native"
            conn.close()
            client(binary, "get", f"https://localhost:{port}/", expect=2)
            if os.name != "nt":
                env = dict(os.environ, SSL_CERT_FILE=args[0])
                assert "200\n" in client(binary,"get",f"https://localhost:{port}/__kinal/debug/server",env=env)
                client(binary,"get",f"https://127.0.0.1:{port}/",env=env,expect=2)
    print("Native HTTP client/server and verified HTTPS passed.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("probe", type=Path)
    run(parser.parse_args().probe.resolve())
