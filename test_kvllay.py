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

def test_atomic_counters_and_rate_limiting(port=6389):
    print(f"\n--- Testing Atomic Counters & Rate Limiting on port {port} ---")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))
    send_recv(s, "FLUSHDB\r\n")

    # 1. INCR on nonexistent key -> initializes to 1
    res = send_recv(s, "INCR counter\r\n")
    assert res == ":1\r\n", f"Expected :1\\r\\n, got {repr(res)}"
    res = send_recv(s, "GET counter\r\n")
    assert res == "$1\r\n1\r\n", f"Expected $1\\r\\n1\\r\\n, got {repr(res)}"
    print("[PASS] INCR initializes nonexistent key to 1")

    # 2. Sequential INCR
    res = send_recv(s, "INCR counter\r\n")
    assert res == ":2\r\n", f"Expected :2\\r\\n, got {repr(res)}"
    res = send_recv(s, "INCR counter\r\n")
    assert res == ":3\r\n", f"Expected :3\\r\\n, got {repr(res)}"
    print("[PASS] Sequential INCR increments key value")

    # 3. DECR on existing key
    res = send_recv(s, "DECR counter\r\n")
    assert res == ":2\r\n", f"Expected :2\\r\\n, got {repr(res)}"
    res = send_recv(s, "DECR counter\r\n")
    assert res == ":1\r\n", f"Expected :1\\r\\n, got {repr(res)}"
    res = send_recv(s, "DECR counter\r\n")
    assert res == ":0\r\n", f"Expected :0\\r\\n, got {repr(res)}"
    res = send_recv(s, "DECR counter\r\n")
    assert res == ":-1\r\n", f"Expected :-1\\r\\n, got {repr(res)}"
    print("[PASS] DECR decrements key value, handles 0 and negative")

    # 4. DECR on nonexistent key -> initializes to -1
    res = send_recv(s, "DECR nonexist_decr\r\n")
    assert res == ":-1\r\n", f"Expected :-1\\r\\n, got {repr(res)}"
    res = send_recv(s, "GET nonexist_decr\r\n")
    assert res == "$2\r\n-1\r\n", f"Expected $2\\r\\n-1\\r\\n, got {repr(res)}"
    print("[PASS] DECR initializes nonexistent key to -1")

    # 5. INCRBY with arbitrary increments
    send_recv(s, "SET num 10\r\n")
    res = send_recv(s, "INCRBY num 5\r\n")
    assert res == ":15\r\n", f"Expected :15\\r\\n, got {repr(res)}"
    res = send_recv(s, "INCRBY num -20\r\n")
    assert res == ":-5\r\n", f"Expected :-5\\r\\n, got {repr(res)}"
    res = send_recv(s, "INCRBY nonexist_incrby 100\r\n")
    assert res == ":100\r\n", f"Expected :100\\r\\n, got {repr(res)}"
    print("[PASS] INCRBY handles positive, negative, and nonexistent keys")

    # 6. DECRBY with arbitrary decrements
    res = send_recv(s, "DECRBY num 5\r\n")
    assert res == ":-10\r\n", f"Expected :-10\\r\\n, got {repr(res)}"
    res = send_recv(s, "DECRBY num -15\r\n")
    assert res == ":5\r\n", f"Expected :5\\r\\n, got {repr(res)}"
    res = send_recv(s, "DECRBY nonexist_decrby 42\r\n")
    assert res == ":-42\r\n", f"Expected :-42\\r\\n, got {repr(res)}"
    print("[PASS] DECRBY handles positive, negative, and nonexistent keys")

    # 7. RESP2 protocol array format for counter commands
    res = send_recv(s, "*2\r\n$4\r\nINCR\r\n$5\r\nrespk\r\n")
    assert res == ":1\r\n", f"Expected :1\\r\\n, got {repr(res)}"
    res = send_recv(s, "*3\r\n$6\r\nINCRBY\r\n$5\r\nrespk\r\n$2\r\n10\r\n")
    assert res == ":11\r\n", f"Expected :11\\r\\n, got {repr(res)}"
    res = send_recv(s, "*2\r\n$4\r\nDECR\r\n$5\r\nrespk\r\n")
    assert res == ":10\r\n", f"Expected :10\\r\\n, got {repr(res)}"
    res = send_recv(s, "*3\r\n$6\r\nDECRBY\r\n$5\r\nrespk\r\n$1\r\n3\r\n")
    assert res == ":7\r\n", f"Expected :7\\r\\n, got {repr(res)}"
    print("[PASS] Counter commands work via RESP2 array protocol")

    # 8. Non-integer error handling
    send_recv(s, "SET str_val \"hello world\"\r\n")
    res = send_recv(s, "INCR str_val\r\n")
    assert "ERR value is not an integer or out of range" in res, f"Expected integer error, got {repr(res)}"
    res = send_recv(s, "DECR str_val\r\n")
    assert "ERR value is not an integer or out of range" in res, f"Expected integer error, got {repr(res)}"
    res = send_recv(s, "INCRBY str_val 10\r\n")
    assert "ERR value is not an integer or out of range" in res, f"Expected integer error, got {repr(res)}"
    res = send_recv(s, "INCRBY counter notanint\r\n")
    assert "ERR value is not an integer or out of range" in res, f"Expected integer error, got {repr(res)}"
    res = send_recv(s, "DECRBY counter notanint\r\n")
    assert "ERR value is not an integer or out of range" in res, f"Expected integer error, got {repr(res)}"
    print("[PASS] Proper error responses for non-integer values and arguments")

    # 9. Overflow and Underflow detection
    send_recv(s, "SET maxint 9223372036854775807\r\n")
    res = send_recv(s, "INCR maxint\r\n")
    assert "ERR increment or decrement would overflow" in res, f"Expected overflow error, got {repr(res)}"
    res = send_recv(s, "INCRBY maxint 1\r\n")
    assert "ERR increment or decrement would overflow" in res, f"Expected overflow error, got {repr(res)}"

    send_recv(s, "SET minint -9223372036854775808\r\n")
    res = send_recv(s, "DECR minint\r\n")
    assert "ERR increment or decrement would overflow" in res, f"Expected overflow error, got {repr(res)}"
    res = send_recv(s, "DECRBY minint 1\r\n")
    assert "ERR increment or decrement would overflow" in res, f"Expected overflow error, got {repr(res)}"
    print("[PASS] Overflow and underflow protection working correctly")

    # 10. Preserves TTL on increment / decrement
    send_recv(s, "SETEX ttl_counter 10 50\r\n")
    res = send_recv(s, "TTL ttl_counter\r\n")
    ttl_before = int(res.strip()[1:])
    assert ttl_before > 0, f"Expected positive TTL, got {ttl_before}"
    res = send_recv(s, "INCR ttl_counter\r\n")
    assert res == ":51\r\n", f"Expected :51\\r\\n, got {repr(res)}"
    res = send_recv(s, "TTL ttl_counter\r\n")
    ttl_after = int(res.strip()[1:])
    assert ttl_after > 0, f"Expected TTL to remain active, got {ttl_after}"
    print("[PASS] INCR/DECR preserves existing key TTL")

    # 11. Rate Limiting Pattern (INCR + EXPIRE fixed-window pattern)
    # Scenario: max 3 requests per 1-second window
    rate_limit_key = "ratelimit:user:42"
    send_recv(s, f"DEL {rate_limit_key}\r\n")
    allowed = 0
    blocked = 0
    LIMIT = 3
    for i in range(5):
        cnt_res = send_recv(s, f"INCR {rate_limit_key}\r\n")
        cnt = int(cnt_res.strip()[1:])
        if cnt == 1:
            send_recv(s, f"EXPIRE {rate_limit_key} 1\r\n")
        if cnt <= LIMIT:
            allowed += 1
        else:
            blocked += 1

    assert allowed == 3, f"Expected 3 allowed requests, got {allowed}"
    assert blocked == 2, f"Expected 2 blocked requests, got {blocked}"
    print("[PASS] Rate Limiter pattern: allowed first 3 requests, throttled 2 requests")

    # Wait for the rate limiting window to expire
    time.sleep(1.2)
    # Next request must reset to 1 and be allowed
    cnt_res = send_recv(s, f"INCR {rate_limit_key}\r\n")
    cnt = int(cnt_res.strip()[1:])
    assert cnt == 1, f"Expected counter to reset to 1 after window expiration, got {cnt}"
    print("[PASS] Rate Limiter pattern: window reset after TTL expiration")

    # 12. Multi-threaded Atomic Concurrency Stress Test
    # 10 threads each performing 100 INCR operations on the same key = exactly 1000
    print("Running multi-threaded atomic concurrency stress test...")
    shared_key = "atomic_stress"
    send_recv(s, f"SET {shared_key} 0\r\n")
    threads = []
    thread_errors = []

    def incr_worker():
        try:
            ws = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            ws.connect(("127.0.0.1", port))
            for _ in range(100):
                r = send_recv(ws, f"INCR {shared_key}\r\n")
                if not r.startswith(":"):
                    thread_errors.append(f"Unexpected response: {r}")
            ws.close()
        except Exception as e:
            thread_errors.append(f"Thread exception: {e}")

    for _ in range(10):
        t = threading.Thread(target=incr_worker)
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    assert not thread_errors, f"Thread errors: {thread_errors}"
    res = send_recv(s, f"GET {shared_key}\r\n")
    assert res == "$4\r\n1000\r\n", f"Expected atomic 1000, got {repr(res)}"
    print("[PASS] Multi-threaded atomicity verified: 10 threads x 100 INCR = exactly 1000")

    # 10 threads each performing 50 DECR operations on the same key = exactly 500
    threads = []
    def decr_worker():
        try:
            ws = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            ws.connect(("127.0.0.1", port))
            for _ in range(50):
                r = send_recv(ws, f"DECR {shared_key}\r\n")
                if not r.startswith(":"):
                    thread_errors.append(f"Unexpected response: {r}")
            ws.close()
        except Exception as e:
            thread_errors.append(f"Thread exception: {e}")

    for _ in range(10):
        t = threading.Thread(target=decr_worker)
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    assert not thread_errors, f"Thread errors: {thread_errors}"
    res = send_recv(s, f"GET {shared_key}\r\n")
    assert res == "$3\r\n500\r\n", f"Expected atomic 500, got {repr(res)}"
    print("[PASS] Multi-threaded atomicity verified: 10 threads x 50 DECR = exactly 500")

    s.close()
    print("[PASS] All Atomic Counter & Rate Limiting tests passed successfully!")

if __name__ == "__main__":
    test_port = int(sys.argv[1]) if len(sys.argv) > 1 else 6389
    test_kvllay(test_port)
    test_ttl(test_port)
    test_atomic_counters_and_rate_limiting(test_port)

