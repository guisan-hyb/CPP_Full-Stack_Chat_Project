#!/usr/bin/env bash
set -u

ROOT="${ROOT:-/home/guisan/ChatPerf}"
RUN_DIR="${ROOT}/perf-run"
pids=""

for name in gate chat2 chat1 status; do
    pid_file="${RUN_DIR}/${name}/service.pid"
    if [ -f "${pid_file}" ]; then
        pid="$(cat "${pid_file}")"
        if kill -0 "${pid}" 2>/dev/null; then
            kill "${pid}" 2>/dev/null || true
            pids="${pids} ${pid}"
        fi
        rm -f "${pid_file}"
    fi
done

for _ in $(seq 1 20); do
    alive=0
    for pid in ${pids}; do
        if kill -0 "${pid}" 2>/dev/null; then
            alive=1
            break
        fi
    done
    [ "${alive}" -eq 0 ] && break
    sleep 0.2
done

for pid in ${pids}; do
    if kill -0 "${pid}" 2>/dev/null; then
        kill -9 "${pid}" 2>/dev/null || true
    fi
done

echo "Performance services stopped."
