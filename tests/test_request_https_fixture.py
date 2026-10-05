from __future__ import annotations

import argparse
import contextlib
import http.client
import io
import json
import os
import socket
import ssl
import stat
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest import mock

import run_tests


class RequestHttpsFixtureTests(unittest.TestCase):
    def connect(self, server, context, hostname="127.0.0.1"):
        raw = socket.create_connection(server.server_address, timeout=5)
        try:
            return context.wrap_socket(raw, server_hostname=hostname)
        except BaseException:
            raw.close()
            raise

    def finish_handshake_probe(self, connection):
        connection.sendall(b"GET /health HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
        response = b""
        while chunk := connection.recv(4096):
            response += chunk
        self.assertTrue(response.endswith(b"ok"))

    def request(self, server, context, method, path, body=None):
        connection = http.client.HTTPSConnection(
            "127.0.0.1", server.server_port, timeout=5, context=context
        )
        try:
            connection.request(method, path, body=body)
            response = connection.getresponse()
            return response.status, response.read(), dict(response.getheaders())
        finally:
            connection.close()

    def test_verified_tls_preserves_endpoints_and_checks_both_sans(self):
        with run_tests.request_https_fixture(port=0) as (server, client):
            self.assertEqual(server.server_address[0], "127.0.0.1")
            self.assertEqual(client.verify_mode, ssl.CERT_REQUIRED)
            self.assertTrue(client.check_hostname)
            self.assertTrue(client.verify_flags & ssl.VERIFY_X509_STRICT)
            self.assertFalse(client.hostname_checks_common_name)
            self.assertIsNone(client.keylog_filename)
            for hostname in ("127.0.0.1", "localhost"):
                with self.subTest(hostname=hostname), self.connect(server, client, hostname) as connection:
                    certificate = connection.getpeercert()
                    self.finish_handshake_probe(connection)
                    self.assertEqual(set(certificate["subjectAltName"]),
                                     {("DNS", "localhost"), ("IP Address", "127.0.0.1")})
                    start = ssl.cert_time_to_seconds(certificate["notBefore"])
                    end = ssl.cert_time_to_seconds(certificate["notAfter"])
                    self.assertEqual(end - start, 24 * 60 * 60)
                    self.assertLessEqual(start, time.time())
                    self.assertGreater(end, time.time())
            status, body, headers = self.request(server, client, "GET", "/hello")
            self.assertEqual((status, body), (200, b"hello-secure"))
            self.assertTrue(headers.get("Content-Type"))
            status, body, headers = self.request(server, client, "POST", "/echo", b"ping")
            self.assertEqual((status, body), (200, b"echo:ping"))
            self.assertGreaterEqual(len(headers), 1)
            self.assertEqual(self.request(server, client, "HEAD", "/hello")[:2], (200, b""))
            self.assertEqual(self.request(server, client, "GET", "/missing")[:2], (404, b"not-found"))

    def test_untrusted_certificate_and_wrong_hostname_are_rejected(self):
        with run_tests.request_https_fixture(port=0) as (server, client):
            untrusted = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
            with self.assertRaises(ssl.SSLCertVerificationError):
                self.connect(server, untrusted)
            with self.assertRaises(ssl.SSLCertVerificationError):
                self.connect(server, client, "wrong.invalid")
            self.assertEqual(self.request(server, client, "GET", "/health")[:2], (200, b"ok"))

    def test_every_invocation_has_a_new_certificate_and_public_key(self):
        certificates = []
        public_keys = []
        for _ in range(2):
            with run_tests.request_https_fixture(port=0) as (server, client):
                with self.connect(server, client) as connection:
                    certificate = connection.getpeercert(binary_form=True)
                    self.finish_handshake_probe(connection)
                certificates.append(certificate)
                # Only public certificate data enters this inspection command.
                result = subprocess.run(
                    [run_tests.shutil.which("openssl"), "x509", "-inform", "DER", "-pubkey", "-noout"],
                    input=certificate, capture_output=True, check=True, timeout=5,
                )
                public_keys.append(result.stdout)
        self.assertNotEqual(certificates[0], certificates[1])
        self.assertNotEqual(public_keys[0], public_keys[1])

    def test_private_files_are_restricted_and_removed_before_serving(self):
        original_run = subprocess.run
        original_directory = tempfile.TemporaryDirectory
        generated_paths = []
        stdout, stderr = io.StringIO(), io.StringIO()
        with original_directory(prefix="kinal fixture parent space ") as parent:
            keylog = Path(parent) / "tls-keys.log"
            random_state = Path(parent) / "random-state"
            def directory(**kwargs):
                return original_directory(dir=parent, **kwargs)
            def execute(command, **kwargs):
                key = Path(command[command.index("-keyout") + 1])
                cert = Path(command[command.index("-out") + 1])
                config = Path(command[command.index("-config") + 1])
                generated_paths.extend((key, cert, config, key.parent))
                self.assertEqual(kwargs["stdin"], subprocess.DEVNULL)
                self.assertEqual(kwargs["stdout"], subprocess.DEVNULL)
                self.assertEqual(kwargs["stderr"], subprocess.DEVNULL)
                self.assertEqual(kwargs["timeout"], 30)
                self.assertFalse(kwargs.get("shell", False))
                if os.name != "nt":
                    self.assertEqual(stat.S_IMODE(key.stat().st_mode), 0o600)
                    self.assertEqual(stat.S_IMODE(key.parent.stat().st_mode), 0o700)
                result = original_run(command, **kwargs)
                if os.name != "nt":
                    self.assertEqual(stat.S_IMODE(key.stat().st_mode), 0o600)
                return result
            with mock.patch.object(run_tests.tempfile, "TemporaryDirectory", side_effect=directory), \
                 mock.patch.object(run_tests.subprocess, "run", side_effect=execute), \
                 mock.patch.dict(os.environ, {"SSLKEYLOGFILE": str(keylog), "RANDFILE": str(random_state)}), \
                 contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                with run_tests.request_https_fixture(port=0) as (server, client):
                    self.assertTrue(generated_paths)
                    self.assertTrue(all(not path.exists() for path in generated_paths))
                    self.assertIsNone(client.keylog_filename)
                    self.assertEqual(self.request(server, client, "GET", "/health")[:2], (200, b"ok"))
            self.assertFalse(keylog.exists())
            self.assertFalse(random_state.exists())
        self.assertEqual(stdout.getvalue(), "")
        self.assertEqual(stderr.getvalue(), "")

    def test_missing_openssl_fails_instead_of_skipping(self):
        with mock.patch.object(run_tests.shutil, "which", return_value=None), \
             mock.patch.object(run_tests.subprocess, "run") as execute:
            with self.assertRaisesRegex(OSError, "OpenSSL command-line tool on PATH"):
                run_tests.request_https_contexts()
            execute.assert_not_called()

    def test_generation_failures_are_clean_and_do_not_echo_process_output(self):
        for failure in ("nonzero", "timeout", "launch", "missing", "malformed"):
            with self.subTest(failure=failure):
                roots = []
                def execute(command, **kwargs):
                    key = Path(command[command.index("-keyout") + 1])
                    cert = Path(command[command.index("-out") + 1])
                    roots.append(key.parent)
                    if failure == "timeout":
                        raise subprocess.TimeoutExpired(command, 30, output="DO_NOT_LOG", stderr="DO_NOT_LOG")
                    if failure == "launch":
                        raise OSError("DO_NOT_LOG")
                    if failure == "malformed":
                        key.write_text("malformed key", encoding="ascii")
                        cert.write_text("malformed certificate", encoding="ascii")
                    return subprocess.CompletedProcess(command, 8 if failure == "nonzero" else 0,
                                                       "DO_NOT_LOG", "DO_NOT_LOG")
                stdout, stderr = io.StringIO(), io.StringIO()
                with mock.patch.object(run_tests.shutil, "which", return_value="openssl"), \
                     mock.patch.object(run_tests.subprocess, "run", side_effect=execute), \
                     contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                    with self.assertRaises(OSError) as caught:
                        run_tests.request_https_contexts()
                self.assertIn("HTTPS fixture", str(caught.exception))
                self.assertNotIn("DO_NOT_LOG", str(caught.exception))
                self.assertEqual(stdout.getvalue() + stderr.getvalue(), "")
                self.assertTrue(roots)
                self.assertTrue(all(not root.exists() for root in roots))

    def test_socket_is_closed_after_normal_exit_and_test_failure(self):
        for fail in (False, True):
            with self.subTest(fail=fail):
                server = None
                try:
                    with run_tests.request_https_fixture(port=0) as (server, _):
                        if fail:
                            raise RuntimeError("body failed")
                except RuntimeError as error:
                    self.assertTrue(fail)
                    self.assertEqual(str(error), "body failed")
                self.assertIsNotNone(server)
                self.assertEqual(server.socket.fileno(), -1)

    def test_socket_is_closed_when_tls_wrap_or_thread_start_fails(self):
        for fail in ("wrap", "thread"):
            with self.subTest(fail=fail):
                server = run_tests.RequestHttpsTestServer(("127.0.0.1", 0), run_tests.RequestHttpsHandler)
                context = mock.Mock()
                context.wrap_socket.side_effect = OSError("wrap failed") if fail == "wrap" else None
                context.wrap_socket.return_value = server.socket
                thread = mock.Mock()
                thread.start.side_effect = RuntimeError("thread failed")
                with mock.patch.object(run_tests, "request_https_contexts", return_value=(context, mock.Mock())), \
                     mock.patch.object(run_tests, "RequestHttpsBoundedServer", return_value=server), \
                     mock.patch.object(run_tests.threading, "Thread", return_value=thread):
                    with self.assertRaises((OSError, RuntimeError)):
                        with run_tests.request_https_fixture(port=0):
                            self.fail("fixture unexpectedly yielded")
                self.assertEqual(server.socket.fileno(), -1)

    def test_bind_and_health_failures_do_not_yield(self):
        with mock.patch.object(run_tests, "request_https_contexts", return_value=(mock.Mock(), mock.Mock())), \
             mock.patch.object(run_tests, "RequestHttpsBoundedServer", side_effect=OSError("bind failed")):
            with self.assertRaisesRegex(OSError, "bind failed"):
                with run_tests.request_https_fixture(port=0):
                    self.fail("fixture unexpectedly yielded")
        original = run_tests.RequestHttpsBoundedServer
        servers = []
        def create(*args, **kwargs):
            server = original(*args, **kwargs)
            servers.append(server)
            return server
        with mock.patch.object(run_tests, "RequestHttpsBoundedServer", side_effect=create), \
             mock.patch.object(run_tests, "verify_request_https_fixture", side_effect=OSError("health failed")):
            with self.assertRaisesRegex(OSError, "health failed"):
                with run_tests.request_https_fixture(port=0):
                    self.fail("fixture unexpectedly yielded")
        self.assertEqual(len(servers), 1)
        self.assertEqual(servers[0].socket.fileno(), -1)

    def test_incomplete_tls_and_http_clients_cannot_hang_cleanup(self):
        # A subprocess deadline makes a cleanup regression fail instead of
        # hanging the whole test run. Keep every client open until fixture exit.
        script = r'''
import socket, ssl, sys, threading, time
sys.path.insert(0, sys.argv[1])
from run_tests import request_https_fixture
baseline = set(threading.enumerate())
connections = []
try:
    with request_https_fixture(port=0) as (server, client):
        connections.append(socket.create_connection(server.server_address, timeout=3))
        for incomplete_body in (False, True):
            raw = socket.create_connection(server.server_address, timeout=3)
            connection = client.wrap_socket(raw, server_hostname="127.0.0.1")
            connections.append(connection)
            if incomplete_body:
                connection.sendall(b"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\nx")
        time.sleep(0.05)
        started = time.monotonic()
    assert time.monotonic() - started < 3, "fixture cleanup exceeded deadline"
    assert server.socket.fileno() == -1, "listener is still open"
    assert not (set(threading.enumerate()) - baseline), "request threads survived cleanup"
    for connection in connections:
        try:
            assert connection.recv(1) == b"", "client connection remained usable"
        except (ConnectionError, ssl.SSLError):
            pass
finally:
    for connection in connections:
        connection.close()
print("bounded TLS and HTTP cleanup passed")
'''
        result = subprocess.run(
            [sys.executable, "-c", script, str(Path(__file__).resolve().parent)],
            capture_output=True, text=True, timeout=10, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "bounded TLS and HTTP cleanup passed\n")
        self.assertEqual(result.stderr, "")

    def test_occupied_port_fails_without_leaving_generated_files(self):
        original_directory = tempfile.TemporaryDirectory
        with original_directory() as parent:
            def directory(**kwargs):
                return original_directory(dir=parent, **kwargs)
            with socket.socket() as occupied:
                occupied.bind(("127.0.0.1", 0))
                occupied.listen()
                with mock.patch.object(run_tests.tempfile, "TemporaryDirectory", side_effect=directory):
                    with self.assertRaises(OSError):
                        with run_tests.request_https_fixture(port=occupied.getsockname()[1]):
                            self.fail("fixture unexpectedly yielded")
                self.assertEqual(list(Path(parent).iterdir()), [])

    def test_unexpected_health_response_closes_connection(self):
        for status, body in ((503, b"ok"), (200, b"wrong")):
            with self.subTest(status=status, body=body):
                connection = mock.Mock()
                connection.getresponse.return_value.status = status
                connection.getresponse.return_value.read.return_value = body
                with mock.patch.object(run_tests.http.client, "HTTPSConnection", return_value=connection):
                    with self.assertRaisesRegex(OSError, "unexpected health response"):
                        run_tests.verify_request_https_fixture(1234, mock.Mock())
                connection.close.assert_called_once()

    def test_c_runner_uses_fixture_and_propagates_missing_tool_failure(self):
        # This verifies runner wiring with an explicit fake compiler/process;
        # it is not a C compiler or Kinal runtime acceptance test.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            compiler = root / "compiler"
            compiler.touch()
            manifest = root / "manifest.json"
            manifest.write_text(json.dumps([{"name": "https", "file": "main.kn", "expected": "ok\n",
                                            "https_fixture": "request_echo"}]), encoding="utf-8")
            args = argparse.Namespace(compiler=str(compiler), manifest=str(manifest), out_dir=str(root / "out"),
                                      driver_checks=False, package_checks=False)
            with mock.patch.object(run_tests, "parse_args", return_value=args), \
                 mock.patch.object(run_tests, "run", return_value=subprocess.CompletedProcess([], 0, "ok\n", "")), \
                 mock.patch.object(run_tests, "request_https_fixture", side_effect=OSError("OpenSSL unavailable")) as fixture, \
                 contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(OSError, "OpenSSL unavailable"):
                    run_tests.main()
            fixture.assert_called_once_with()

    def test_both_consumers_use_the_live_verified_fixture(self):
        # Exercise each consumer with a simulated executable whose output is
        # derived from real fixture requests. Compiler execution is not claimed.
        selfhost = Path(__file__).resolve().parent / "selfhost"
        sys.path.insert(0, str(selfhost))
        try:
            import audit_manifest_runtime as audit
        finally:
            sys.path.pop(0)
        original_fixture = run_tests.request_https_fixture
        original_run = subprocess.run
        live = []
        @contextlib.contextmanager
        def fixture():
            with original_fixture(port=0) as pair:
                live.append(pair)
                yield pair
        def output(command, **kwargs):
            server, client = live[-1]
            status, body, _ = self.request(server, client, "GET", "/hello")
            self.assertEqual((status, body), (200, b"hello-secure"))
            return subprocess.CompletedProcess(command, 0, "hello-secure\n", "")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            compiler = root / "compiler"
            compiler.touch()
            executable = root / "program"
            case = {"name": "https", "file": "main.kn", "expected": "hello-secure\n",
                    "https_fixture": "request_echo"}
            manifest = root / "manifest.json"
            manifest.write_text(json.dumps([case]), encoding="utf-8")
            args = argparse.Namespace(compiler=str(compiler), manifest=str(manifest), out_dir=str(root / "out"),
                                      driver_checks=False, package_checks=False)
            def compiler_runner(command, **kwargs):
                return output(command, **kwargs) if len(command) == 1 else subprocess.CompletedProcess(command, 0)
            with mock.patch.object(run_tests, "parse_args", return_value=args), \
                 mock.patch.object(run_tests, "run", side_effect=compiler_runner), \
                 mock.patch.object(run_tests, "request_https_fixture", side_effect=fixture), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(run_tests.main(), 0)
            def selfhost_runner(command, **kwargs):
                return output(command, **kwargs) if command == [str(executable)] else original_run(command, **kwargs)
            with mock.patch.object(run_tests, "request_https_fixture", side_effect=fixture), \
                 mock.patch.object(audit.subprocess, "run", side_effect=selfhost_runner):
                ok, detail = audit.run_case(root, root, executable, case, run_tests)
            self.assertTrue(ok, detail)
            self.assertEqual(len(live), 2)
            self.assertTrue(all(server.socket.fileno() == -1 for server, _ in live))

    def test_selfhost_consumer_reports_or_propagates_fixture_failure(self):
        selfhost = Path(__file__).resolve().parent / "selfhost"
        sys.path.insert(0, str(selfhost))
        try:
            import audit_manifest_runtime as audit
        finally:
            sys.path.pop(0)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            case = {"expected": "", "https_fixture": "request_echo"}
            with mock.patch.object(run_tests, "request_https_fixture", side_effect=OSError("OpenSSL unavailable")):
                try:
                    ok, detail = audit.run_case(root, root, root / "program", case, run_tests)
                except OSError as error:
                    # Earlier review layers predate per-case OSError reporting.
                    # They must still fail loudly, never return a pass or skip.
                    self.assertIn("OpenSSL unavailable", str(error))
                else:
                    self.assertFalse(ok)
                    self.assertIn("OpenSSL unavailable", detail)


if __name__ == "__main__":
    unittest.main()
