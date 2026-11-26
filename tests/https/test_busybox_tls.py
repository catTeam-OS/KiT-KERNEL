#!/usr/bin/env python3
import argparse
import os
import socket
import ssl
import subprocess
import tempfile
import threading
import time


HTTP_RESPONSE = (
    b"HTTP/1.1 200 OK\r\n"
    b"Content-Length: 2\r\n"
    b"Connection: close\r\n"
    b"\r\n"
    b"OK"
)


class LocalTLSServer:
    def __init__(self, cert, key, cipher=None, group=None, stall=False):
        self.stall = stall
        self.context = None
        if not stall:
            self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            self.context.minimum_version = ssl.TLSVersion.TLSv1_2
            self.context.maximum_version = ssl.TLSVersion.TLSv1_2
            self.context.set_ciphers(cipher)
            self.context.set_ecdh_curve(group)
            self.context.load_cert_chain(cert, key)
        self.listener = socket.socket()
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(1)
        self.listener.settimeout(6)
        self.port = self.listener.getsockname()[1]
        self.request_received = False
        self.error = None
        self.thread = threading.Thread(target=self._serve, daemon=True)
        self.thread.start()

    def _serve(self):
        try:
            raw, _ = self.listener.accept()
            raw.settimeout(5)
            with raw:
                if self.stall:
                    time.sleep(2)
                    return
                with self.context.wrap_socket(raw, server_side=True) as conn:
                    request = bytearray()
                    while b"\r\n\r\n" not in request:
                        chunk = conn.recv(4096)
                        if not chunk:
                            break
                        request.extend(chunk)
                    self.request_received = b"\r\n\r\n" in request
                    if self.request_received:
                        conn.sendall(HTTP_RESPONSE)
        except (OSError, ssl.SSLError) as exc:
            self.error = exc
        finally:
            self.listener.close()

    def close(self):
        self.listener.close()
        self.thread.join(timeout=6)


def run_wget(binary, url, timeout):
    with tempfile.TemporaryDirectory() as empty_path:
        env = os.environ.copy()
        env["PATH"] = empty_path
        for name in (
            "http_proxy",
            "https_proxy",
            "HTTP_PROXY",
            "HTTPS_PROXY",
            "all_proxy",
            "ALL_PROXY",
        ):
            env.pop(name, None)
        return subprocess.run(
            [
                binary,
                "wget",
                "-T",
                str(timeout),
                "-S",
                "-O",
                os.devnull,
                url,
            ],
            env=env,
            capture_output=True,
            text=True,
            timeout=timeout + 5,
            check=False,
        )


def assert_https_download(binary, cert, key, cipher, group):
    server = LocalTLSServer(cert, key, cipher=cipher, group=group)
    try:
        result = run_wget(binary, f"https://127.0.0.1:{server.port}/", 5)
        output = result.stdout + result.stderr
        server.thread.join(timeout=6)
        if result.returncode != 0 or "200 OK" not in output:
            raise AssertionError(
                f"{cipher} with {group} failed (exit {result.returncode}):\n"
                f"{output}\nserver error: {server.error}"
            )
        if not server.request_received:
            raise AssertionError("TLS completed but the server received no HTTP request")
        if "TLS certificate validation not implemented" not in output:
            raise AssertionError("the internal TLS certificate-validation warning disappeared")
    finally:
        server.close()


def assert_timeout_is_not_a_crash(binary):
    server = LocalTLSServer(None, None, stall=True)
    try:
        result = run_wget(binary, f"https://127.0.0.1:{server.port}/", 1)
        output = result.stdout + result.stderr
        if result.returncode == 0:
            raise AssertionError("wget unexpectedly succeeded against a stalled TLS peer")
        if result.returncode < 0:
            raise AssertionError(
                f"wget crashed with signal {-result.returncode} on timeout:\n{output}"
            )
        if "download timed out" not in output:
            raise AssertionError(f"timeout diagnostic missing:\n{output}")
    finally:
        server.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--busybox", required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as certdir:
        cert = os.path.join(certdir, "cert.pem")
        key = os.path.join(certdir, "key.pem")
        subprocess.run(
            [
                "openssl",
                "req",
                "-x509",
                "-newkey",
                "ec",
                "-pkeyopt",
                "ec_paramgen_curve:prime256v1",
                "-nodes",
                "-days",
                "1",
                "-subj",
                "/CN=localhost",
                "-keyout",
                key,
                "-out",
                cert,
            ],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        assert_https_download(
            args.busybox,
            cert,
            key,
            "ECDHE-ECDSA-AES128-GCM-SHA256",
            "X25519",
        )
        assert_https_download(
            args.busybox,
            cert,
            key,
            "ECDHE-ECDSA-AES128-GCM-SHA256",
            "prime256v1",
        )
        assert_https_download(
            args.busybox,
            cert,
            key,
            "ECDHE-ECDSA-AES128-SHA256",
            "X25519",
        )

    assert_timeout_is_not_a_crash(args.busybox)
    print("BusyBox TLS GCM, CBC control, and timeout tests passed.")


if __name__ == "__main__":
    main()
