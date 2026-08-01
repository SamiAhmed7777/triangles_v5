FROM ubuntu:22.04 AS builder

ARG VERSION=6.2.0
ARG DEB_URL=https://github.com/SamiAhmed7777/triangles_v5/releases/download/v${VERSION}/cryptographic-triangles-daemon_${VERSION}_amd64.deb

RUN apt-get update && apt-get install -y --no-install-recommends \
        curl ca-certificates binutils zstd && \
    curl -fsSL -o /tmp/triangles.deb "${DEB_URL}" && \
    cd /tmp && ar x /tmp/triangles.deb && \
    tar --use-compress-program=unzstd -xf data.tar.zst && \
    rm -f /tmp/triangles.deb /tmp/control.tar.zst /tmp/debian-binary /tmp/data.tar.zst

# ---------- Runtime ----------
FROM ubuntu:22.04

ARG VERSION=6.2.0

LABEL maintainer="Cryptographic Triangles Team"
LABEL description="Cryptographic Triangles (TRI) headless daemon"
LABEL version="6.2.0"

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        libssl3 \
        libevent-2.1-7 \
        libboost-system1.74.0 \
        libboost-filesystem1.74.0 \
        libboost-program-options1.74.0 \
        libboost-thread1.74.0 \
        libboost-chrono1.74.0 \
        libdb5.3++ \
        libminiupnpc17 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /tmp/usr/lib/cryptographic-triangles/ /opt/triangles/
COPY --from=builder /tmp/usr/bin/trianglesd /usr/local/bin/trianglesd
COPY --from=builder /tmp/usr/bin/triangles-cli /usr/local/bin/triangles-cli

# Wrapper sets LD_LIBRARY_PATH so the dynamic libs resolve
RUN printf '#!/bin/bash\nexport LD_LIBRARY_PATH=/opt/triangles/lib:${LD_LIBRARY_PATH}\nexec /opt/triangles/%s "$@"\n' trianglesd \
        > /usr/local/bin/trianglesd-wrap && \
    printf '#!/bin/bash\nexport LD_LIBRARY_PATH=/opt/triangles/lib:${LD_LIBRARY_PATH}\nexec /opt/triangles/%s "$@"\n' triangles-cli \
        > /usr/local/bin/triangles-cli-wrap && \
    mv /usr/local/bin/trianglesd-wrap /usr/local/bin/trianglesd && \
    mv /usr/local/bin/triangles-cli-wrap /usr/local/bin/triangles-cli && \
    chmod +x /usr/local/bin/trianglesd /usr/local/bin/triangles-cli

RUN useradd -m -s /bin/bash triangles && \
    mkdir -p /home/triangles/.triangles && \
    chown -R triangles:triangles /home/triangles

USER triangles
WORKDIR /home/triangles
VOLUME /home/triangles/.triangles
EXPOSE 24112 19112

ENTRYPOINT ["trianglesd"]
CMD ["-daemon=0", "-printtoconsole"]
