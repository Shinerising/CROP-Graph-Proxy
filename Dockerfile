# Cross-compile
FROM phusion/baseimage:jammy-1.0.1 AS cross-compile
WORKDIR /App
COPY . ./
RUN apt-get update && \
    apt-get install -y --no-install-recommends build-essential cmake mingw-w64-i686-dev mingw-w64-tools g++-mingw-w64-i686 && \
    cmake -DCMAKE_TOOLCHAIN_FILE=mingw-w64-i686.cmake . && make

FROM phusion/baseimage:jammy-1.0.1

# Set correct environment variables
ENV DEBIAN_FRONTEND noninteractive
ENV LC_ALL zh_CN.UTF-8
ENV LANG zh_CN.UTF-8
ENV LANGUAGE zh_CN.UTF-8
ENV WINEARCH win32
ENV DISPLAY :0
ENV WINEPREFIX /home/docker/.wine
ENV HOME /home/docker/
ENV NOVNC_HOME /usr/libexec/noVNCdim

# Updating and upgrading a bit.
# Install vnc, window manager and basic tools
RUN apt-get update && \
    apt-get install -y --no-install-recommends cron language-pack-zh-hans x11vnc supervisor scrot sudo && \
    dpkg --add-architecture i386 && \
# We need software-properties-common to add ppas.
    curl https://dl.winehq.org/wine-builds/winehq.key -o /tmp/Release.key && \
    apt-get install -y --no-install-recommends software-properties-common && \
    apt-key add /tmp/Release.key && \
    apt-add-repository 'https://dl.winehq.org/wine-builds/ubuntu/' && \
    add-apt-repository ppa:cybermax-dexter/sdl2-backport && \
    apt-get update && \
    apt-get install -y --no-install-recommends winehq-stable && \
    apt-get install -y --no-install-recommends xvfb python3 && \
# Install winetricks
    curl -SL -k https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks  -o /usr/local/bin/winetricks && \
    chmod a+x /usr/local/bin/winetricks  && \
# Installation of winbind to stop ntlm error messages.
    apt-get install -y --no-install-recommends winbind && \
# Add Traditional Chinese Fonts
    mkdir -p /usr/share/fonts/TTF/ && \
    curl -SL -k https://github.com/adobe-fonts/source-han-sans/raw/release/OTF/SimplifiedChinese/SourceHanSansSC-Regular.otf -o /usr/share/fonts/TTF/SourceHanSansTC-Regular.otf && \
    curl -SL -k https://github.com/adobe-fonts/source-han-sans/raw/release/OTF/SimplifiedChinese/SourceHanSansSC-Bold.otf -o /usr/share/fonts/TTF/SourceHanSansTC-Bold.otf && \
# Create user for ssh
    adduser \
            --home /home/docker \
            --disabled-password \
            --shell /bin/bash \
            --gecos "user for running application" \
            --quiet \
            docker && \
    echo "docker:1234" | chpasswd && \
    adduser docker sudo && \
# Clone noVNC
    mkdir -p "${NOVNC_HOME}"/utils/websockify && \
    curl -L https://github.com/novnc/noVNC/archive/v1.3.0.tar.gz | tar xz --strip 1 -C "${NOVNC_HOME}" && \
    curl -L https://github.com/novnc/websockify/archive/v0.10.0.tar.gz | tar xz --strip 1 -C "${NOVNC_HOME}"/utils/websockify && \
    chmod +x -v "${NOVNC_HOME}"/utils/novnc_proxy && \
    ln -s "${NOVNC_HOME}"/vnc.html "${NOVNC_HOME}"/index.html && \
    chown -R docker "${NOVNC_HOME}" && \
# Cleaning up.
    apt-get autoremove -y --purge && \
    apt-get clean -y && \
    rm -rf /home/wine/.cache && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# Add supervisor conf
COPY supervisord.conf /etc/supervisor/conf.d/supervisord.conf

# Add entrypoint.sh
COPY entrypoint.sh /etc/entrypoint.sh

# Copy fonts
COPY ./fonts/ /usr/share/fonts/TTF/

# Copy novnc files
COPY ./novnc/ /usr/libexec/noVNCdim/

# Add compiled app
COPY --from=cross-compile /App/out/ /home/docker/.wine/drive_c/

# Copy static
RUN mkdir /home/web

# Add cron job
COPY screenshot-cron /etc/cron.d/screenshot-cron

ENTRYPOINT ["/bin/bash","/etc/entrypoint.sh"]
# Expose Port
EXPOSE 8000 8080 22
