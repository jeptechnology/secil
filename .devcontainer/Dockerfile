FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

# Install apt dependencies
RUN \
   apt-get update \
   && apt-get install --yes --no-install-recommends --no-install-suggests \
      zip \
      cmake \
      g++ \
      git \
      git-lfs \
      jq \
      locales \
      make \
      ninja-build \
      nanopb \
   && apt-get clean --quiet=2 \
   && apt-get autoremove --quiet=2 \
   && locale-gen en_US en_US.UTF-8 \
   && rm --recursive --force /var/lib/apt/lists/* /tmp/*
