import base64
import pathlib
import re
import struct


ROOT = pathlib.Path(__file__).resolve().parents[2]


def read_text(path):
    return (ROOT / path).read_text(encoding="utf-8", errors="ignore")


def assert_define(path, name, value):
    text = read_text(path)
    pattern = rf"#define\s+{re.escape(name)}\s+{re.escape(str(value))}\b"
    assert re.search(pattern, text), f"{path} must define {name} as {value}"


def assert_contains(path, token):
    text = read_text(path)
    assert token in text, f"{path} must contain {token}"


def test_header_can_carry_large_json_body():
    msg_id = 1020
    body = b"x" * (70 * 1024)
    packet = struct.pack("!HI", msg_id, len(body)) + body

    parsed_id, parsed_len = struct.unpack("!HI", packet[:6])
    assert parsed_id == msg_id
    assert parsed_len == len(body)
    assert packet[6:] == body


def test_base64_chunks_reassemble_binary_payload():
    payload = bytes(range(256)) * 17
    chunks = [payload[i:i + 2048] for i in range(0, len(payload), 2048)]
    encoded = [base64.b64encode(chunk).decode("ascii") for chunk in chunks]
    decoded = b"".join(base64.b64decode(chunk) for chunk in encoded)
    assert decoded == payload


def test_source_protocol_constants_match():
    assert_contains("client/llfcchat/global.h", "ID_UPLOAD_FILE_REQ")
    assert_contains("client/llfcchat/global.h", "ID_UPLOAD_FILE_RSP")
    assert_contains("client/llfcchat/global.h", "TCP_HEAD_TOTAL_LEN = 6")
    assert_contains("client/llfcchat/global.h", "TCP_HEAD_DATA_LEN = 4")

    for server in ("server/ChatServer", "server/ChatServer2"):
        assert_define(f"{server}/const.h", "HEAD_TOTAL_LEN", 6)
        assert_define(f"{server}/const.h", "HEAD_DATA_LEN", 4)
        assert_contains(f"{server}/const.h", "ID_UPLOAD_FILE_REQ")
        assert_contains(f"{server}/const.h", "ID_UPLOAD_FILE_RSP")


def main():
    tests = [
        test_header_can_carry_large_json_body,
        test_base64_chunks_reassemble_binary_payload,
        test_source_protocol_constants_match,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")


if __name__ == "__main__":
    main()
