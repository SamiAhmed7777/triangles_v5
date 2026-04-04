FROM ubuntu:22.04

LABEL maintainer="Cryptographic Triangles Team"
LABEL description="Cryptographic Triangles (TRI) headless daemon"
LABEL version="5.5.5"

ARG VERSION=5.3.7

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
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
    tor \
    && rm -rf /var/lib/apt/lists/*

RUN curl -L -o /usr/local/bin/trianglesd \
    "https://github.com/SamiAhmed7777/triangles_v5/releases/download/v${VERSION}/Cryptographic-Triangles-v${VERSION}-linux-x64-daemon" \
    && chmod +x /usr/local/bin/trianglesd

RUN useradd -m -s /bin/bash triangles

USER triangles
WORKDIR /home/triangles

RUN mkdir -p /home/triangles/.triangles

VOLUME /home/triangles/.triangles

EXPOSE 24112 19112

ENTRYPOINT ["trianglesd"]
CMD ["-daemon=0", "-printtoconsole"]
