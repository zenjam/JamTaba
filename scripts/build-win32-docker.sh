#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE_NAME="${IMAGE_NAME:-jamtaba-win32-build}"
PLATFORM="${DOCKER_PLATFORM:-linux/amd64}"
LOG_FILE="${LOG_FILE:-/tmp/jamtaba-win32-docker-build.log}"
RUN_LOG_FILE="${RUN_LOG_FILE:-/tmp/jamtaba-win32-container-build.log}"
VERBOSE="${VERBOSE:-0}"

format_duration() {
    local total_seconds="$1"
    local hours=$((total_seconds / 3600))
    local minutes=$(((total_seconds % 3600) / 60))
    local seconds=$((total_seconds % 60))
    printf "%02dh:%02dm:%02ds" "${hours}" "${minutes}" "${seconds}"
}

SCRIPT_START_TIME="$(date +%s)"

echo "Building Docker image ${IMAGE_NAME}"
echo "Build log: ${LOG_FILE}"
BUILD_START_TIME="$(date +%s)"

if [ "${VERBOSE}" = "1" ]; then
    docker build \
        --platform "${PLATFORM}" \
        --progress=plain \
        -t "${IMAGE_NAME}" \
        -f "${ROOT_DIR}/docker/win32/Dockerfile" \
        "${ROOT_DIR}" 2>&1 | tee "${LOG_FILE}"
    BUILD_STATUS=${PIPESTATUS[0]}
else
    docker build \
        --platform "${PLATFORM}" \
        --progress=plain \
        -t "${IMAGE_NAME}" \
        -f "${ROOT_DIR}/docker/win32/Dockerfile" \
        "${ROOT_DIR}" >"${LOG_FILE}" 2>&1
    BUILD_STATUS=$?
fi

if [ "${BUILD_STATUS}" -ne 0 ]; then
    echo "Docker image build failed"
    echo "Elapsed: $(format_duration "$(( $(date +%s) - BUILD_START_TIME ))")"
    echo "See: ${LOG_FILE}"
    tail -40 "${LOG_FILE}" || true
    exit "${BUILD_STATUS}"
fi

echo "Docker image build finished"
echo "Image build time: $(format_duration "$(( $(date +%s) - BUILD_START_TIME ))")"
echo "Running JamTaba win32 build inside container"
echo "Container build log: ${RUN_LOG_FILE}"
RUN_START_TIME="$(date +%s)"

set +e
if [ "${VERBOSE}" = "1" ]; then
    docker run \
        --rm \
        --platform "${PLATFORM}" \
        --entrypoint /bin/bash \
        -v "${ROOT_DIR}:/workspace" \
        -w /workspace/PROJECTS \
        "${IMAGE_NAME}" \
        -lc 'find /workspace/PROJECTS \( -name Makefile -o -name ".qmake.stash" -o -name "*.o" -o -name "moc_*.cpp" -o -name "moc_predefs.h" \) -delete; \
             qmake Jamtaba.pro CONFIG+=release && \
             make -j"$(nproc)"' 2>&1 | tee "${RUN_LOG_FILE}"
    RUN_STATUS=${PIPESTATUS[0]}
else
    docker run \
        --rm \
        --platform "${PLATFORM}" \
        --entrypoint /bin/bash \
        -v "${ROOT_DIR}:/workspace" \
        -w /workspace/PROJECTS \
        "${IMAGE_NAME}" \
        -lc 'find /workspace/PROJECTS \( -name Makefile -o -name ".qmake.stash" -o -name "*.o" -o -name "moc_*.cpp" -o -name "moc_predefs.h" \) -delete; \
             qmake Jamtaba.pro CONFIG+=release && \
             make -j"$(nproc)"' >"${RUN_LOG_FILE}" 2>&1
    RUN_STATUS=$?
fi
set -e

if [ "${RUN_STATUS}" -ne 0 ]; then
    echo
    echo "JamTaba win32 build failed. First likely error lines:"
    echo "Container build time: $(format_duration "$(( $(date +%s) - RUN_START_TIME ))")"
    echo "Total elapsed: $(format_duration "$(( $(date +%s) - SCRIPT_START_TIME ))")"
    grep -nE "error:|Error |No rule to make target|cannot find|undefined reference|file format not recognized|fatal:|make(\[[0-9]+\])?: \*\*\*" "${RUN_LOG_FILE}" | head -80 || true
    echo
    echo "Full log: ${RUN_LOG_FILE}"
    exit "${RUN_STATUS}"
fi

echo "JamTaba win32 build finished"
echo "Container build time: $(format_duration "$(( $(date +%s) - RUN_START_TIME ))")"
echo "Total elapsed: $(format_duration "$(( $(date +%s) - SCRIPT_START_TIME ))")"
