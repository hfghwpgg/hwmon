import json
import socket
from time import sleep


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


def set_interval(sock):
    send_data(sock, '{"cmd": "set_interval"}')


def get_data(sock):
    send_data(sock, '{"cmd": "get"}')


def reset(sock):
    send_data(sock, '{"cmd": "reset"}')


with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
    sock.connect("/tmp/hwmon.sock")

    # send_ping(sock)
    # set_interval(sock)
    get_data(sock)
    reset(sock)
    sleep(1)
    get_data(sock)
