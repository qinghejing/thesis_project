# Chat File Transfer Smoke Tests

These tests cover the protocol assumptions used by the first-stage chat file upload feature:

- chat TCP frames use a 6-byte big-endian header: `uint16 message id + uint32 body length`
- file chunks can carry base64 payloads and reassemble into the original binary bytes
- the client and both chat servers expose matching upload message IDs and header constants

Run from the repository root:

```powershell
python tests/chat_file_transfer/protocol_smoke.py
```
