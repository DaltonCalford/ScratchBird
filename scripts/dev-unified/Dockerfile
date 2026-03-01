FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      acl \
      autoconf \
      automake \
      bash \
      bash-completion \
      ca-certificates \
      ca-certificates-java \
      clang \
      clang-format \
      cmake \
      ccache \
      curl \
      fakeroot \
      g++ \
      gcc \
      gfortran \
      git \
      git-lfs \
      gnupg \
      golang-go \
      gzip \
      htop \
      iproute2 \
      iputils-ping \
      jq \
      less \
      lsb-release \
      libboost-all-dev \
      libbz2-dev \
      libc6-dev \
      libffi-dev \
      libgdbm-dev \
      libicu-dev \
      liblz4-dev \
      liblzma-dev \
      libncurses5-dev \
      libncursesw5-dev \
      libreadline-dev \
      libsqlite3-dev \
      libssl-dev \
      libtool \
      libuuid1 \
      libxml2-dev \
      libxslt1-dev \
      libcurl4-openssl-dev \
      locales \
      make \
      m4 \
      ninja-build \
      netcat-openbsd \
      npm \
      nodejs \
      openjdk-17-jdk \
      openjdk-17-jre-headless \
      openssl \
      openssh-server \
      openssh-client \
      psmisc \
      patch \
      pkg-config \
      postgresql-client \
      procps \
      protobuf-compiler \
      python3 \
      python3-dev \
      python3-pip \
      python3-venv \
      python3-setuptools \
      python3-wheel \
      build-essential \
      ruby \
      rustc \
      cargo \
      software-properties-common \
      sqlite3 \
      sudo \
      tar \
      unzip \
      uuid-dev \
      wget \
      which \
      zip \
      zlib1g \
      zlib1g-dev \
      composer \
      php-cli \
      fpc \
      ant \
      maven \
      gradle \
      yarn \
      file \
      gettext-base \
      bison \
      flex \
      rsync \
      && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

RUN if id -u 1000 >/dev/null 2>&1; then \
      if ! id -u builder >/dev/null 2>&1; then \
        useradd --create-home --shell /bin/bash builder; \
      fi; \
    else \
      useradd --uid 1000 --create-home --shell /bin/bash builder; \
    fi && \
    usermod -aG sudo builder && \
    mkdir -p /workspace /var/run/sshd && \
    chown builder:builder /workspace

WORKDIR /workspace

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
COPY build-matrix.sh /usr/local/bin/build-matrix.sh
RUN chmod +x /usr/local/bin/entrypoint.sh /usr/local/bin/build-matrix.sh

ENV WORKDIR_PATH=/workspace
ENV LISTEN_IP=0.0.0.0
ENV SSH_PORT=22
ENV SSH_USER=builder

EXPOSE 22

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
