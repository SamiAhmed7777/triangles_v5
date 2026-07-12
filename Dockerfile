FROM ubuntu:24.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive
ARG SOURCE_DATE_EPOCH=1700000000
ENV SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH}

RUN apt-get update && apt-get install -y --no-install-recommends \
    autoconf \
    automake \
    build-essential \
    ca-certificates \
    cmake \
    libboost-all-dev \
    libdb++-dev \
    libevent-dev \
    libleveldb-dev \
    liblz4-dev \
    liblzma-dev \
    libminiupnpc-dev \
    librocksdb-dev \
    libsnappy-dev \
    libsqlite3-dev \
    libssl-dev \
    libtool \
    libzstd-dev \
    ninja-build \
    pkg-config \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN test -s src/secp256k1/CMakeLists.txt \
    && test -s src/tor/tor-src/configure.ac

RUN LIBEVENT_DIR=/usr OPENSSL_DIR=/usr ZLIB_DIR=/usr \
    bash src/tor/build-libtor.sh

RUN cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_QT=OFF \
      -DBUILD_DAEMON=ON \
      -DBUILD_CLI=ON \
      -DBUILD_TESTS=OFF \
      -DUSE_UPNP=OFF \
      -DUSE_I2P_EMBEDDED=OFF \
    && cmake --build build --parallel 2

RUN install -D -m 0755 build/bin/trianglesd /opt/triangles/bin/trianglesd \
    && install -D -m 0755 build/bin/triangles-cli /opt/triangles/bin/triangles-cli \
    && mkdir -p /opt/triangles/rootfs \
    && { ldd /opt/triangles/bin/trianglesd; ldd /opt/triangles/bin/triangles-cli; } \
       | awk '/=> \// {print $3} /^\// {print $1}' \
       | sort -u \
       | while IFS= read -r library; do \
           cp --parents -L "${library}" /opt/triangles/rootfs; \
         done

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid 10001 triangles \
    && useradd --uid 10001 --gid triangles --home-dir /var/lib/triangles \
         --no-create-home --shell /usr/sbin/nologin triangles \
    && install -d -m 0700 -o triangles -g triangles /var/lib/triangles

COPY --from=builder /opt/triangles/rootfs/ /
COPY --from=builder /opt/triangles/bin/ /usr/local/bin/
RUN ldconfig

USER triangles
WORKDIR /var/lib/triangles

EXPOSE 24112
VOLUME ["/var/lib/triangles"]
STOPSIGNAL SIGTERM

ENTRYPOINT ["/usr/local/bin/trianglesd"]
CMD ["-datadir=/var/lib/triangles", "-printtoconsole", "-upnp=0", "-rest=0", "-rpcbind=127.0.0.1"]
