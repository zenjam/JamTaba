FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
ENV JAMTABA_BUILDER=docker

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y \
        autoconf \
        build-essential \
        cmake \
        freeglut3-dev \
        gdb \
        git \
        iputils-ping \
        libasound2-dev \
        libfontconfig1-dev \
        libgl1-mesa-dev \
        libglu1-mesa-dev \
        libjpeg-dev \
        libnss3-dev \
        libopus-dev \
        libogg-dev \
        libpng-dev \
        libssl-dev \
        libtool \
        libvorbis-dev \
        libx11-xcb-dev \
        libxcb-xinerama0-dev \
        libxcb-xkb-dev \
        libxcb1-dev \
        libxcursor-dev \
        libxi-dev \
        libxkbcommon-dev \
        libxkbcommon-x11-dev \
        libxml2-dev \
        libxrender-dev \
        libxslt-dev \
        libqt5websockets5-dev \
        lzip \
        mesa-common-dev \
        nano \
        perl \
        python \
        qt5-qmake \
        qtbase5-dev \
        qtbase5-dev-tools \
        qtmultimedia5-dev \
        qttools5-dev-tools \
        qtchooser \
        valgrind \
        wget \
        zlib1g-dev \
        '^libxcb.*-dev' && \
    apt-get autoremove -y && \
    apt-get autoclean -y && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/*

RUN set -eux && \
    cd /opt && \
    git clone --depth 1 --branch stable https://code.videolan.org/videolan/x264.git && \
    cd /opt/x264 && \
    ./configure --host=x86_64-linux-gnu --enable-static --disable-asm --disable-opencl && \
    make -j"$(nproc)" && \
    make install && \
    rm -rf /opt/x264

RUN set -eux && \
    cd /opt && \
    git clone --depth 1 --branch n4.4.1 https://git.ffmpeg.org/ffmpeg.git && \
    cd /opt/ffmpeg && \
    ./configure --arch=x86_64 --target-os=linux --enable-static --enable-pic --disable-x86asm --disable-doc --disable-programs && \
    make -j"$(nproc)" install && \
    rm -rf /opt/ffmpeg

RUN set -eux && \
    cd /opt && \
    git clone --depth 1 --branch v19.7.0 https://github.com/PortAudio/portaudio.git && \
    cd /opt/portaudio && \
    ./configure --host=x86_64-linux-gnu --enable-static --enable-shared=false --with-alsa --without-oss && \
    make -j"$(nproc)" install && \
    rm -rf /opt/portaudio

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
