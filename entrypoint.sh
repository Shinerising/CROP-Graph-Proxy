#!/bin/bash
function check_vnc_pass {
  if [[ -n $VNC_PASSWORD ]]; then
    local VNC_PASSWD_PATH="/home/docker/.vnc/passwd"
    rm -f $VNC_PASSWD_PATH
    if [[ ! -d "/home/docker/.vnc" ]]; then mkdir -p /home/docker/.vnc;fi
    x11vnc -storepasswd "$VNC_PASSWORD" "$VNC_PASSWD_PATH"
    export X11_ARGS="-forever -shared -viewonly -rfbauth $VNC_PASSWD_PATH"
    chown docker $VNC_PASSWD_PATH
    chmod 600 $VNC_PASSWD_PATH
  else
    export X11_ARGS="-forever -shared -viewonly"
  fi
}
if [ ! -f /etc/ssh/ssh_host_rsa_key ]; then
  # Generate ssh key
  ssh-keygen -N '' -t rsa -f /etc/ssh/ssh_host_rsa_key
  ssh-keygen -N '' -t dsa -f /etc/ssh/ssh_host_dsa_key
  ssh-keygen -N '' -t ecdsa -f /etc/ssh/ssh_host_ecdsa_key
fi

check_vnc_pass

if [[ -n $USER_PASSWORD ]];then
  echo "docker:$USER_PASSWORD" | chpasswd
fi
if [[ -n $PUID ]]; then
  usermod -u "$PUID" docker
fi
if [[ -n $PGID ]]; then
  groupmod -g "$PGID" docker
fi

export WINEDLLOVERRIDES="mscoree,mshtml="

/usr/bin/wine REG ADD "HKCU\Software\Wine\WineDbg" /v "ShowCrashDialog" /t REG_DWORD /d 0 /f

crontab /etc/cron.d/screenshot-cron && cron -f &

/usr/bin/supervisord
