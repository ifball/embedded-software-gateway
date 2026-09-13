import socket
import sys


def main() -> int:
    host = "127.0.0.1"
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9000

    with socket.create_connection((host, port), timeout=5) as sock:
        print(sock.recv(1024).decode("utf-8", errors="replace").strip())
        print("Try: status | sensor | version | led on | led off | pwm 50 | stats | reset_error | quit")

        while True:
            try:
                line = input("pc> ").strip()
            except EOFError:
                line = "quit"

            if not line:
                continue

            sock.sendall((line + "\n").encode("utf-8"))
            data = sock.recv(1024)
            if not data:
                print("gateway disconnected")
                break
            print(data.decode("utf-8", errors="replace").strip())

            if line in {"quit", "exit"}:
                break

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
