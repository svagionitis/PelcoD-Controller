#!/usr/bin/env python3
"""ONVIF Profile S Automated Conformance Runner.

Spawns OnvifServer, executes domain pytest test suites, validates responses
against strict W3C XML schemas, and generates JUnit XML test reports.
"""

from __future__ import annotations

import argparse
import os
import socket
import subprocess
import sys
import time
from typing import Optional


def find_free_port() -> int:
    """Probe OS for an available ephemeral TCP port."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


class OnvifServerSupervisor:
    """Manages the lifecycle of the OnvifServer binary."""

    def __init__(self, binary_path: str, port: int) -> None:
        self.binary_path = binary_path
        self.port = port
        self.process: Optional[subprocess.Popen] = None

    def start(self) -> None:
        """Launch the OnvifServer process and wait for listening socket."""
        cmd = [self.binary_path, "--port", str(self.port), "--test-mode"]
        print(f"[*] Spawning server: {' '.join(cmd)}")
        self.process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        self._wait_until_ready(timeout_sec=5.0)

    def _wait_until_ready(self, timeout_sec: float) -> None:
        """Poll the server port until open."""
        start_time = time.time()
        while time.time() - start_time < timeout_sec:
            if self.process and self.process.poll() is not None:
                stdout, stderr = self.process.communicate()
                raise RuntimeError(
                    f"Server process terminated prematurely with code {self.process.returncode}:\n"
                    f"STDOUT: {stdout}\nSTDERR: {stderr}"
                )
            try:
                with socket.create_connection(("127.0.0.1", self.port), timeout=0.2):
                    return
            except (ConnectionRefusedError, OSError):
                time.sleep(0.05)
        raise TimeoutError(f"Server failed to bind to port {self.port} within {timeout_sec}s")

    def stop(self) -> None:
        """Gracefully terminate server process."""
        if self.process:
            print("[*] Terminating server process...")
            self.process.terminate()
            try:
                self.process.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                print("[!] Force killing server process...")
                self.process.kill()
                self.process.wait()


def main() -> int:
    parser = argparse.ArgumentParser(description="ONVIF Profile S Conformance Runner")
    parser.add_argument(
        "--server-binary",
        help="Path to OnvifServer executable (if omitted, connects to existing target)",
    )
    parser.add_argument("--host", default="127.0.0.1", help="Target server IP or hostname")
    parser.add_argument("--port", type=int, default=0, help="Target server port (0 = ephemeral)")
    parser.add_argument(
        "--output-junit",
        default="conformance_report.xml",
        help="Output path for JUnit XML report",
    )
    parser.add_argument("-k", "--keyword", help="Filter pytest test cases by keyword")
    args = parser.parse_args()

    port = args.port
    supervisor: Optional[OnvifServerSupervisor] = None

    if args.server_binary:
        if port == 0:
            port = find_free_port()
        supervisor = OnvifServerSupervisor(args.server_binary, port)
    elif port == 0:
        port = 8080

    suites_dir = pathlib_suites_dir = os.path.join(os.path.dirname(__file__), "suites")

    try:
        if supervisor:
            supervisor.start()

        print(f"[*] Running ONVIF Conformance Suite against {args.host}:{port}")
        os.environ["ONVIF_TEST_HOST"] = args.host
        os.environ["ONVIF_TEST_PORT"] = str(port)

        pytest_cmd = [
            sys.executable,
            "-m",
            "pytest",
            suites_dir,
            f"--junitxml={args.output_junit}",
            "-v",
        ]
        if args.keyword:
            pytest_cmd.extend(["-k", args.keyword])

        result = subprocess.run(pytest_cmd)
        return result.returncode
    finally:
        if supervisor:
            supervisor.stop()


if __name__ == "__main__":
    sys.exit(main())
