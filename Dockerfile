# Cross-compile
FROM phusion/baseimage:jammy-1.0.1 AS cross-compile
WORKDIR /App
COPY . ./
RUN apt-get update && \
    apt-get install -y --no-install-recommends build-essential cmake mingw-w64-i686-dev mingw-w64-tools g++-mingw-w64-i686 && \
    cmake -DCMAKE_TOOLCHAIN_FILE=mingw-w64-i686.cmake . && make

FROM ghcr.io/shinerising/wine-with-vnc:v0.0.6

# Set correct environment variables
ENV REQUEST_HOST localhost
ENV REQUEST_PORT 80
ENV REQUEST_PATH api/graph/simple
ENV STATION test
ENV TEST_MODE false

# Add supervisor conf
COPY supervisord.conf /etc/supervisor/conf.d/supervisord.conf

# Add entrypoint.sh
COPY entrypoint.sh /etc/entrypoint.sh

# Copy novnc files
COPY ./novnc/ /usr/libexec/noVNCdim/

# Add compiled app
COPY --from=cross-compile /App/out/ /home/docker/.wine/drive_c/

# Copy static
RUN mkdir /home/web

# Add cron job
COPY screenshot-cron /etc/cron.d/screenshot-cron

# Entry point
ENTRYPOINT ["/bin/bash","/etc/entrypoint.sh"]

# Expose Port
EXPOSE 8000 8080

# Volumes
VOLUME ["/home/docker/.wine/drive_c/graph"]
