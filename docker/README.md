JamTaba Docker Build Images

These Dockerfiles are a repo-owned replacement for the external
`defter/jamtaba-*` build images referenced by the GitHub Actions workflows.

Targets:

- `docker/linux/Dockerfile`: native Linux build image
- `docker/win64/Dockerfile`: Linux-hosted MXE cross-build image for Windows x64
- `docker/win32/Dockerfile`: Linux-hosted MXE cross-build image for Windows x86

Notes:

- The Windows images are not Windows containers. They are Ubuntu 20.04 images
  using MXE to cross-build Windows binaries.
- The original Defter images were built around Qt 5.15.7 and set
  `JAMTABA_BUILDER=docker`.
- The ASIO SDK download URL may change over time. If it does, set
  `ASIO_SDK_URL` during `docker build`.

Typical usage:

```bash
./scripts/build-linux-docker.sh
./scripts/build-win64-docker.sh
./scripts/build-win32-docker.sh
./scripts/build-macos.sh
```
