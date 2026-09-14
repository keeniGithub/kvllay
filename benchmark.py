import socket
import time
import threading
import sys
import statistics

def send_command(sock, cmd_bytes):
    sock.sendall(cmd_bytes)
    resp = sock.recv(1024)
    return resp

def benchmark_single(port=6379, password=None, iterations=10000):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', port))
    if password:
        send_command(s, f"AUTH {password}\r\n".encode())

    print(f"\n[1] Single-connection Benchmark ({iterations} ops each):")

    # SET benchmark
    set_payload = b"*3\r\n$3\r\nSET\r\n$7\r\ntestkey\r\n$9\r\ntestvalue\r\n"
    start = time.perf_counter()
    for _ in range(iterations):
        send_command(s, set_payload)
    elapsed = time.perf_counter() - start
    set_rps = iterations / elapsed
    print(f"  SET: {set_rps:,.0f} req/sec (avg latency: {elapsed / iterations * 1000:.3f} ms)")

    # GET benchmark
    get_payload = b"*2\r\n$3\r\nGET\r\n$7\r\ntestkey\r\n"
    start = time.perf_counter()
    for _ in range(iterations):
        send_command(s, get_payload)
    elapsed = time.perf_counter() - start
    get_rps = iterations / elapsed
    print(f"  GET: {get_rps:,.0f} req/sec (avg latency: {elapsed / iterations * 1000:.3f} ms)")

    # LPUSH benchmark
    lpush_payload = b"*3\r\n$5\r\nLPUSH\r\n$7\r\nlistkey\r\n$5\r\nitem1\r\n"
    start = time.perf_counter()
    for _ in range(iterations):
        send_command(s, lpush_payload)
    elapsed = time.perf_counter() - start
    lpush_rps = iterations / elapsed
    print(f"  LPUSH: {lpush_rps:,.0f} req/sec (avg latency: {elapsed / iterations * 1000:.3f} ms)")

    # LPOP benchmark
    lpop_payload = b"*2\r\n$4\r\nLPOP\r\n$7\r\nlistkey\r\n"
    start = time.perf_counter()
    for _ in range(iterations):
        send_command(s, lpop_payload)
    elapsed = time.perf_counter() - start
    lpop_rps = iterations / elapsed
    print(f"  LPOP: {lpop_rps:,.0f} req/sec (avg latency: {elapsed / iterations * 1000:.3f} ms)")

    s.close()

def benchmark_concurrent(port=6379, password=None, num_threads=10, ops_per_thread=2000):
    total_ops = num_threads * ops_per_thread
    print(f"\n[2] Concurrent Benchmark ({num_threads} parallel threads x {ops_per_thread} ops = {total_ops} total ops):")

    # Concurrent SET
    errors = []
    latencies_set = []

    def set_worker():
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect(('127.0.0.1', port))
            if password:
                send_command(s, f"AUTH {password}\r\n".encode())
            payload = b"*3\r\n$3\r\nSET\r\n$7\r\ntestkey\r\n$9\r\ntestvalue\r\n"
            for _ in range(ops_per_thread):
                t0 = time.perf_counter()
                send_command(s, payload)
                latencies_set.append((time.perf_counter() - t0) * 1000)
            s.close()
        except Exception as e:
            errors.append(e)

    start = time.perf_counter()
    threads = [threading.Thread(target=set_worker) for _ in range(num_threads)]
    for t in threads: t.start()
    for t in threads: t.join()
    elapsed_set = time.perf_counter() - start
    set_rps = total_ops / elapsed_set
    p50_set = statistics.median(latencies_set) if latencies_set else 0
    p99_set = statistics.quantiles(latencies_set, n=100)[98] if len(latencies_set) >= 100 else 0
    print(f"  Parallel SET: {set_rps:,.0f} req/sec | Latency: p50={p50_set:.3f}ms, p99={p99_set:.3f}ms")

    # Concurrent GET
    latencies_get = []
    def get_worker():
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect(('127.0.0.1', port))
            if password:
                send_command(s, f"AUTH {password}\r\n".encode())
            payload = b"*2\r\n$3\r\nGET\r\n$7\r\ntestkey\r\n"
            for _ in range(ops_per_thread):
                t0 = time.perf_counter()
                send_command(s, payload)
                latencies_get.append((time.perf_counter() - t0) * 1000)
            s.close()
        except Exception as e:
            errors.append(e)

    start = time.perf_counter()
    threads = [threading.Thread(target=get_worker) for _ in range(num_threads)]
    for t in threads: t.start()
    for t in threads: t.join()
    elapsed_get = time.perf_counter() - start
    get_rps = total_ops / elapsed_get
    p50_get = statistics.median(latencies_get) if latencies_get else 0
    p99_get = statistics.quantiles(latencies_get, n=100)[98] if len(latencies_get) >= 100 else 0
    print(f"  Parallel GET: {get_rps:,.0f} req/sec | Latency: p50={p50_get:.3f}ms, p99={p99_get:.3f}ms")

    if errors:
        print(f"Errors occurred: {errors}")

if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 6379
    pwd = sys.argv[2] if len(sys.argv) > 2 else None
    print(f"=== Running kvllay Benchmark on port {port} ===")
    benchmark_single(port, pwd, iterations=5000)
    benchmark_concurrent(port, pwd, num_threads=8, ops_per_thread=2000)
    benchmark_concurrent(port, pwd, num_threads=32, ops_per_thread=1000)
