#!/usr/bin/env bash
set -euo pipefail

ROOT="${ROOT:-/home/guisan/ChatPerf}"
RUN_DIR="${ROOT}/perf-run"
BUILD_DIR="${ROOT}/build/linux-release"
VCPKG_LIB="/opt/vcpkg/installed/x64-linux/lib"
FIXTURES="${ROOT}/server/tests/perf-config"

"${ROOT}/server/tests/perf-stop-services.sh" >/dev/null 2>&1 || true
mkdir -p "${RUN_DIR}"/{status,chat1,chat2,gate}
cp "${FIXTURES}/status.ini" "${RUN_DIR}/status/config.ini"
cp "${FIXTURES}/chat1.ini" "${RUN_DIR}/chat1/config.ini"
cp "${FIXTURES}/chat2.ini" "${RUN_DIR}/chat2/config.ini"
cp "${FIXTURES}/gate.ini" "${RUN_DIR}/gate/config.ini"

start_service() {
    local name="$1"
    local executable="$2"
    local workdir="${RUN_DIR}/${name}"
    (
        cd "${workdir}"
        nohup env LD_LIBRARY_PATH="${VCPKG_LIB}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
            "${executable}" >"${workdir}/service.log" 2>&1 &
        echo $! >"${workdir}/service.pid"
    )
}

wait_port() {
    local name="$1"
    local port="$2"
    for _ in $(seq 1 50); do
        if timeout 1 bash -c "</dev/tcp/127.0.0.1/${port}" 2>/dev/null; then
            return 0
        fi
        local pid
        pid="$(cat "${RUN_DIR}/${name}/service.pid")"
        if ! kill -0 "${pid}" 2>/dev/null; then
            tail -80 "${RUN_DIR}/${name}/service.log" >&2 || true
            return 1
        fi
        sleep 0.2
    done
    tail -80 "${RUN_DIR}/${name}/service.log" >&2 || true
    return 1
}

start_service status "${BUILD_DIR}/server/StatusServer/StatusServer"
wait_port status 50052

start_service chat1 "${BUILD_DIR}/server/ChatServer/ChatServer"
wait_port chat1 8990

start_service chat2 "${BUILD_DIR}/server/ChatServer2/ChatServer2"
wait_port chat2 8991

start_service gate "${BUILD_DIR}/server/GateServer/GateServer"
wait_port gate 8080

echo "Performance services are ready."
