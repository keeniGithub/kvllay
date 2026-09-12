import socket
import time
import threading
import sys

def send_recv(sock, data):
    sock.sendall(data.encode('utf-8') if isinstance(data, str) else data)
    return sock.recv(4096).decode('utf-8')

def test_kvllay(port=6389):
    print(f"Connecting to kvllay on port {port}...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))

    # Test 1: PING (inline)
    res = send_recv(s, "PING\r\n")
    assert res == "+PONG\r\n", f"PING inline failed: {repr(res)}"
    print("[PASS] PING (inline)")

    # Test 2: PING with message (inline)
    res = send_recv(s, "PING hello\r\n")
    assert res == "$5\r\nhello\r\n", f"PING msg failed: {repr(res)}"
    print("[PASS] PING with message")

    # Test 3: SET and GET (inline)
    res = send_recv(s, 'SET mykey "hello world"\r\n')
    assert res == "+OK\r\n", f"SET failed: {repr(res)}"
    res = send_recv(s, "GET mykey\r\n")
    assert res == "$11\r\nhello world\r\n", f"GET failed: {repr(res)}"
    print("[PASS] SET and GET (inline)")

    # Test 4: RESP Array format (what redis-cli sends)
    res = send_recv(s, "*3\r\n$3\r\nSET\r\n$4\r\nuser\r\n$5\r\nalice\r\n")
    assert res == "+OK\r\n", f"RESP SET failed: {repr(res)}"
    
    res = send_recv(s, "*2\r\n$3\r\nGET\r\n$4\r\nuser\r\n")
    assert res == "$5\r\nalice\r\n", f"RESP GET failed: {repr(res)}"
    print("[PASS] SET and GET (RESP array protocol)")

    # Test 5: Non-existent key (GET returns $-1\r\n)
    res = send_recv(s, "GET nonexistent\r\n")
    assert res == "$-1\r\n", f"GET null failed: {repr(res)}"
    print("[PASS] GET non-existent (null bulk string)")

    # Test 6: EXISTS
    res = send_recv(s, "EXISTS mykey user nonexistent\r\n")
    assert res == ":2\r\n", f"EXISTS failed: {repr(res)}"
    print("[PASS] EXISTS")

    # Test 7: KEYS
    res = send_recv(s, "KEYS *\r\n")
    assert res.startswith("*2\r\n"), f"KEYS * failed: {repr(res)}"
    assert "mykey" in res and "user" in res
    print("[PASS] KEYS *")

    # Test 8: DBSIZE
    res = send_recv(s, "DBSIZE\r\n")
    assert res == ":2\r\n", f"DBSIZE failed: {repr(res)}"
    print("[PASS] DBSIZE")

    # Test 9: DEL
    res = send_recv(s, "DEL mykey nonexistent\r\n")
    assert res == ":1\r\n", f"DEL failed: {repr(res)}"
    res = send_recv(s, "GET mykey\r\n")
    assert res == "$-1\r\n", f"GET after DEL failed: {repr(res)}"
    print("[PASS] DEL")

    # Test 10: FLUSHDB
    res = send_recv(s, "FLUSHDB\r\n")
    assert res == "+OK\r\n", f"FLUSHDB failed: {repr(res)}"
    res = send_recv(s, "DBSIZE\r\n")
    assert res == ":0\r\n", f"DBSIZE after FLUSHDB failed: {repr(res)}"
    print("[PASS] FLUSHDB")

    # Test 11: COMMAND (redis-cli handshake)
    res = send_recv(s, "COMMAND\r\n")
    assert res == "*0\r\n", f"COMMAND failed: {repr(res)}"
    print("[PASS] COMMAND (handshake)")

    # Test 12: INFO
    res = send_recv(s, "INFO\r\n")
    assert "kvllay_version:1.0.0" in res, f"INFO failed: {repr(res)}"
    print("[PASS] INFO")

    # Test 13: QUIT
    res = send_recv(s, "QUIT\r\n")
    assert res == "+OK\r\n", f"QUIT failed: {repr(res)}"
    rem = s.recv(1024)
    assert len(rem) == 0, f"Expected socket close, got {repr(rem)}"
    s.close()
    print("[PASS] QUIT and connection close")

    print("\n--- Testing Multi-Client Concurrency ---")
    threads = []
    errors = []

    def worker(client_id):
        try:
            ws = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            ws.connect(("127.0.0.1", port))
            for i in range(50):
                k = f"k_{client_id}_{i}"
                v = f"v_{client_id}_{i}"
                send_recv(ws, f"SET {k} {v}\r\n")
                r = send_recv(ws, f"GET {k}\r\n")
                expected = f"${len(v)}\r\n{v}\r\n"
                if r != expected:
                    errors.append(f"Mismatch in worker {client_id}: got {r}, expected {expected}")
            ws.close()
        except Exception as e:
            errors.append(f"Worker {client_id} exception: {e}")

    for cid in range(10):
        t = threading.Thread(target=worker, args=(cid,))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    assert not errors, f"Concurrency errors: {errors}"
    print("[PASS] 10 concurrent clients x 50 operations = 500 requests succeeded")

def test_auth(port=6390, password="secret123"):
    print(f"\n--- Testing Password Protection on port {port} ---")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))

    # Try command without AUTH -> expect NOAUTH
    res = send_recv(s, "PING\r\n")
    assert "NOAUTH" in res, f"Expected NOAUTH, got: {repr(res)}"
    print("[PASS] Rejected command without auth (NOAUTH)")

    # Try AUTH with wrong password -> expect WRONGPASS
    res = send_recv(s, "AUTH wrongpass\r\n")
    assert "WRONGPASS" in res, f"Expected WRONGPASS, got: {repr(res)}"
    print("[PASS] Rejected wrong password (WRONGPASS)")

    # Still unauthorized
    res = send_recv(s, "SET a 1\r\n")
    assert "NOAUTH" in res, f"Expected NOAUTH, got: {repr(res)}"

    # Try AUTH with correct password -> expect +OK
    res = send_recv(s, f"AUTH {password}\r\n")
    assert res == "+OK\r\n", f"Expected +OK, got: {repr(res)}"
    print("[PASS] Authenticated successfully with password")

    # Commands now succeed
    res = send_recv(s, "SET a 123\r\n")
    assert res == "+OK\r\n", f"Expected +OK, got: {repr(res)}"
    res = send_recv(s, "GET a\r\n")
    assert res == "$3\r\n123\r\n", f"Expected $3\\r\\n123\\r\\n, got: {repr(res)}"
    print("[PASS] Commands work after authentication")

    s.close()
    print("[PASS] All AUTH tests passed!")

def test_ttl(port=6389):
    print(f"\n--- Testing TTL & Expiration on port {port} ---")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))

    send_recv(s, "FLUSHDB\r\n")

    res = send_recv(s, "TTL nonexistent\r\n")
    assert res == ":-2\r\n", f"Expected -2 for nonexistent TTL, got {res}"
    res = send_recv(s, "PTTL nonexistent\r\n")
    assert res == ":-2\r\n", f"Expected -2 for nonexistent PTTL, got {res}"
    print("[PASS] TTL and PTTL on nonexistent key returns -2")

    send_recv(s, "SET k1 v1\r\n")
    res = send_recv(s, "TTL k1\r\n")
    assert res == ":-1\r\n", f"Expected -1 for persistent key TTL, got {res}"
    res = send_recv(s, "PTTL k1\r\n")
    assert res == ":-1\r\n", f"Expected -1 for persistent key PTTL, got {res}"
    print("[PASS] Persistent key returns -1")

    res = send_recv(s, "EXPIRE k1 2\r\n")
    assert res == ":1\r\n", f"Expected 1 from EXPIRE, got {res}"
    res = send_recv(s, "TTL k1\r\n")
    assert res in (":2\r\n", ":1\r\n"), f"Expected 1 or 2 for TTL, got {res}"
    res = send_recv(s, "PTTL k1\r\n")
    pttl_val = int(res.strip()[1:])
    assert 0 < pttl_val <= 2000, f"Expected 0 < PTTL <= 2000, got {pttl_val}"
    print("[PASS] EXPIRE, TTL, and PTTL work correctly")

    res = send_recv(s, "PERSIST k1\r\n")
    assert res == ":1\r\n", f"Expected 1 from PERSIST, got {res}"
    res = send_recv(s, "TTL k1\r\n")
    assert res == ":-1\r\n", f"Expected -1 after PERSIST, got {res}"
    res = send_recv(s, "PERSIST k1\r\n")
    assert res == ":0\r\n", f"Expected 0 from PERSIST on persistent key, got {res}"
    print("[PASS] PERSIST removes TTL and returns 0 when no TTL")

    res = send_recv(s, "PEXPIRE k1 400\r\n")
    assert res == ":1\r\n", f"Expected 1 from PEXPIRE, got {res}"
    res = send_recv(s, "PTTL k1\r\n")
    pttl_val = int(res.strip()[1:])
    assert 0 < pttl_val <= 400, f"Expected 0 < PTTL <= 400, got {pttl_val}"
    time.sleep(0.5)
    res = send_recv(s, "GET k1\r\n")
    assert res == "$-1\r\n", f"Expected null after expiration, got {res}"
    res = send_recv(s, "TTL k1\r\n")
    assert res == ":-2\r\n", f"Expected -2 after expiration, got {res}"
    print("[PASS] PEXPIRE and lazy eviction on GET")

    res = send_recv(s, "SETEX k2 1 hello_ttl\r\n")
    assert res == "+OK\r\n", f"Expected +OK from SETEX, got {res}"
    res = send_recv(s, "GET k2\r\n")
    assert res == "$9\r\nhello_ttl\r\n", f"Expected value from SETEX, got {res}"
    res = send_recv(s, "TTL k2\r\n")
    assert res in (":1\r\n", ":2\r\n"), f"Expected 1 from TTL after SETEX, got {res}"
    time.sleep(1.2)
    res = send_recv(s, "EXISTS k2\r\n")
    assert res == ":0\r\n", f"Expected 0 from EXISTS after expiration, got {res}"
    print("[PASS] SETEX and lazy eviction on EXISTS")

    send_recv(s, "SETEX k_reset 10 initial\r\n")
    res = send_recv(s, "TTL k_reset\r\n")
    assert int(res.strip()[1:]) > 0, f"Expected positive TTL, got {res}"
    send_recv(s, "SET k_reset permanent\r\n")
    res = send_recv(s, "TTL k_reset\r\n")
    assert res == ":-1\r\n", f"Expected -1 after overwriting with SET, got {res}"
    print("[PASS] SET clears existing TTL")

    send_recv(s, "SET k_del 123\r\n")
    res = send_recv(s, "EXPIRE k_del 0\r\n")
    assert res == ":1\r\n", f"Expected 1 from EXPIRE 0, got {res}"
    res = send_recv(s, "EXISTS k_del\r\n")
    assert res == ":0\r\n", f"Expected key to be deleted by EXPIRE 0, got {res}"
    print("[PASS] EXPIRE with 0 deletes key immediately")

    send_recv(s, "FLUSHDB\r\n")
    send_recv(s, "SETEX auto1 1 v\r\n")
    send_recv(s, "SETEX auto2 1 v\r\n")
    send_recv(s, "SETEX auto3 1 v\r\n")
    res = send_recv(s, "DBSIZE\r\n")
    assert res == ":3\r\n", f"Expected 3 keys, got {res}"
    time.sleep(1.3)
    res = send_recv(s, "DBSIZE\r\n")
    assert res == ":0\r\n", f"Expected active eviction to clear all expired keys, got {res}"
    print("[PASS] Active background eviction sweeps expired keys automatically")

    s.close()
    print("[PASS] All TTL & Expiration tests passed!")

if __name__ == "__main__":
    test_port = int(sys.argv[1]) if len(sys.argv) > 1 else 6389
    test_kvllay(test_port)
    test_ttl(test_port)

