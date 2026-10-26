# Build environment for Ginkgo CADx.
#
# Debian 10 (buster) is the last Debian release that shipped ginkgocadx, and the
# last one with the old dependency stack this code needs: VTK 6 (legacy OpenGL
# backend), ITK 4, wxWidgets 3.0 and DCMTK 3.6.4. Buster is end-of-life, so
# packages come from archive.debian.org.
# ITK 4 was only built for amd64 and i386 in buster, so the image is amd64.
FROM --platform=linux/amd64 debian:buster

RUN printf '%s\n' \
      'deb http://archive.debian.org/debian buster main' \
      'deb http://archive.debian.org/debian-security buster/updates main' \
      > /etc/apt/sources.list \
 && echo 'Acquire::Check-Valid-Until "false";' > /etc/apt/apt.conf.d/99archive \
 && apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      build-essential \
      cmake \
      pkg-config \
      libcairo2-dev \
      libcurl4-gnutls-dev \
      libdcmtk-dev \
      libfftw3-dev \
      libfreetype6-dev \
      libgtk2.0-dev \
      libinsighttoolkit4-dev \
      libjsoncpp-dev \
      default-libmysqlclient-dev \
      libsqlite3-dev \
      libvtk6-dev \
      libwrap0 \
      libwrap0-dev \
      libwxgtk3.0-dev \
      libxml2-dev \
      libgdcm-tools \
      locales \
 && sed -i 's/^# *\(en_US.UTF-8\)/\1/' /etc/locale.gen \
 && locale-gen \
 && rm -rf /var/lib/apt/lists/*

ENV LANG=en_US.UTF-8

WORKDIR /src
