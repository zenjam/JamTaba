FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV MXE_COMMIT=a8c8e7823d4e7fa772884fa6b60ee031e33934c6
ENV MXE_TARGET=i686-w64-mingw32.static
ENV PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/opt/mxe/usr/bin
ENV JAMTABA_BUILDER=docker

ARG ASIO_SDK_URL=https://www.steinberg.net/asiosdk

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y \
        autoconf \
        automake \
        autopoint \
        bash \
        bison \
        build-essential \
        bzip2 \
        flex \
        g++ \
        g++-multilib \
        gettext \
        git \
        gperf \
        intltool \
        iputils-ping \
        libc6-dev-i386 \
        libffi-dev \
        libgdk-pixbuf2.0-dev \
        libgl-dev \
        libltdl-dev \
        libssl-dev \
        libtool-bin \
        libxml-parser-perl \
        lzip \
        make \
        nano \
        openssl \
        p7zip-full \
        patch \
        perl \
        pkg-config \
        python-is-python3 \
        python3 \
        python3-mako \
        python3-pkg-resources \
        python3-setuptools \
        ruby \
        scons \
        sed \
        unzip \
        wget \
        xz-utils \
        zip && \
    apt-get autoremove -y && \
    apt-get autoclean -y && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/*

RUN set -eux && \
    mkdir -p /opt/mxe && \
    cd /opt/mxe && \
    git init && \
    git remote add origin https://github.com/mxe/mxe.git && \
    git pull --depth 1 origin "${MXE_COMMIT}"

RUN set -eux && \
    cd /opt/mxe && \
    NPROC="$(nproc)" && \
    make -j1 JOBS="${NPROC}" MXE_TARGETS="${MXE_TARGET}" MXE_USE_CCACHE= qtbase qtwebsockets qtmultimedia qttools || \
    { \
        FAILED_LOG="$(ls -t /opt/mxe/log/*_"${MXE_TARGET}" 2>/dev/null | head -1 || true)" && \
        test -n "${FAILED_LOG}" && echo "[mxe-log] ${FAILED_LOG}" && cat "${FAILED_LOG}"; \
        exit 1; \
    } && \
    ln -s "/opt/mxe/usr/bin/${MXE_TARGET}-qmake-qt5" /usr/bin/qmake && \
    qmake --version

RUN set -eux && \
    for tool in \
        addr2line ar as c++ c++filt cmake cpack cpp dlltool dllwrap elfedit \
        g++ gcc gcc-11.3.0 gcc-ar gcc-nm gcc-ranlib gcov gcov-dump gcov-tool \
        gfortran gprof ld ld.bfd lto-dump meson nm objcopy objdump pcre-config \
        pcre2-config pg_config pkg-config ranlib readelf size strings strip \
        windmc windres; do \
        ln -sf "/opt/mxe/usr/bin/${MXE_TARGET}-${tool}" "/usr/bin/${tool}"; \
    done

RUN set -eux && \
    cd /opt && \
    wget -O asiosdk.zip "${ASIO_SDK_URL}" && \
    unzip -o asiosdk.zip -d . && \
    ASIO_DIR="$(find /opt -maxdepth 1 -type d \( -name 'asiosdk*' -o -name 'ASIOSDK*' \) ! -name 'asiosdk2' | head -1)" && \
    test -n "${ASIO_DIR}" && \
    mv "${ASIO_DIR}" asiosdk2 && \
    rm asiosdk.zip

RUN set -eux && \
    cd /opt && \
    git clone --depth 1 --branch stable https://code.videolan.org/videolan/x264.git && \
    cd /opt/x264 && \
    ./configure --host=mingw32 --enable-static --disable-asm --disable-opencl && \
    make -j"$(nproc)" && \
    make install && \
    rm -rf /opt/x264

RUN set -eux && \
    cd /opt && \
    git clone --depth 1 --branch n4.4.1 https://git.ffmpeg.org/ffmpeg.git && \
    cd /opt/ffmpeg && \
    ./configure --arch=x86 --target-os=mingw32 --enable-cross-compile --enable-static --enable-pic --disable-x86asm --disable-inline-asm --disable-doc --disable-programs && \
    make -j"$(nproc)" install && \
    rm -rf /opt/ffmpeg

RUN set -eux && \
    cd /opt && \
    git clone --depth 1 --branch v19.7.0 https://github.com/PortAudio/portaudio.git && \
    cd /opt/portaudio && \
    ./configure --host=i686-w64-mingw32 --with-winapi=asio --with-asiodir=/opt/asiosdk2 && \
    make -j"$(nproc)" install && \
    rm -rf /opt/asiosdk2 /opt/portaudio

RUN set -eux && \
    cd /opt && \
    git clone --depth 1 --branch v1.3.5 https://gitlab.xiph.org/xiph/ogg && \
    cd /opt/ogg && \
    ./autogen.sh && \
    ./configure --prefix=/usr/local --host=i686-w64-mingw32 --enable-static --enable-shared=false && \
    make -j"$(nproc)" install && \
    rm -rf /opt/ogg

RUN set -eux && \
    cd /opt && \
    git clone --depth 1 --branch v1.3.7 https://gitlab.xiph.org/xiph/vorbis && \
    cd /opt/vorbis && \
    ./autogen.sh && \
    ./configure --prefix=/usr/local --host=i686-w64-mingw32 --enable-static --enable-shared=false && \
    make -j"$(nproc)" install && \
    rm -rf /opt/vorbis

RUN set -eux && \
    cd /opt && \
    git clone --depth 1 --branch miniupnpc_2_2_4 https://github.com/miniupnp/miniupnp && \
    cd /opt/miniupnp/miniupnpc && \
    sed -i 's/DESTINATION include\/miniupnpc/DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/g' CMakeLists.txt && \
    cmake \
        -D CMAKE_INSTALL_INCLUDEDIR=/usr/local/include \
        -D CMAKE_INSTALL_LIBDIR=/usr/local/lib \
        -D UPNPC_BUILD_STATIC=true \
        -D UPNPC_BUILD_SHARED=false \
        -D UPNPC_BUILD_TESTS=false \
        -D UPNPC_BUILD_SAMPLE=false \
        . && \
    make -j"$(nproc)" install && \
    ./updateminiupnpcstrings.sh && \
    cp miniupnpcstrings.h /usr/local/include && \
    rm -rf /opt/miniupnp

ENTRYPOINT ["/bin/bash"]
