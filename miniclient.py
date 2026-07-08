import json
import socket


def receive_data(sock):
    try:
        fp = sock.makefile("r", encoding="utf-8")
        line = fp.readline()
        if not line:
            return None
        return json.loads(line.strip())
    except Exception as e:
        print(f"Error recieving: {e}")
        return None


def send_data(sock, data_str):
    if not data_str.endswith("\n"):
        data_str += "\n"

    sock.sendall(data_str.encode("utf-8"))
    print(receive_data(sock))


def send_ping(sock):
    send_data(sock, '{"cmd": "ping"}')


def set_interval(sock, i):
    send_data(sock, f'{{"cmd": "set_interval", "value": {i}}}')


def get_data(sock):
    send_data(sock, '{"cmd": "get"}')


def reset(sock):
    send_data(sock, '{"cmd": "reset"}')


with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
    sock.connect("/tmp/hwmon.sock")

    get_data(sock)
    set_interval(sock, 1000)
    # reset(sock)
