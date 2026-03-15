FROM ubuntu:22.04

LABEL maintainer="Cryptographic Triangles Team"
LABEL description="Cryptographic Triangles (TRI) headless daemon"
LABEL version="5.1.4"

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    libssl3 \
    libdb5.3++ \
    libboost-system1.74.0 \
    libboost-filesystem1.74.0 \
    libboost-program-options1.74.0 \
    libboost-thread1.74.0 \
    libboost-chrono1.74.0 \
    libevent-2.1-7 \
    libminiupnpc17 \
    tor \
    && rm -rf /var/lib/apt/lists/*

RUN curl -L -o /usr/local/bin/trianglesd \
    https://github.com/SamiAhmed7777/triangles_v5/releases/download/v5.1.4/trianglesd-linux \
    && chmod +x /usr/local/bin/trianglesd

RUN useradd -m -s /bin/bash triangles
USER triangles
WORKDIR /home/triangles

RUN mkdir -p .triangles

EXPOSE 24112 19112

VOLUME ["/home/triangles/.triangles"]

ENTRYPOINT ["trianglesd"]
CMD ["-printtoconsole", "-txindex=1"]
