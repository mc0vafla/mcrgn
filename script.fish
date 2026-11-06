#!/usr/bin/env fish

set -gx YDOTOOL_SOCKET /tmp/.ydotool_socket

sudo pkill ydotoold 2>/dev/null
sudo rm -f /tmp/.ydotool_socket

sudo ydotoold --socket-path=/tmp/.ydotool_socket --socket-perm=0666 >/dev/null 2>&1 &

set -l timeout 0
while not test -S /tmp/.ydotool_socket
    sleep 0.05
    set timeout (math $timeout + 1)
    if test $timeout -gt 30
        exit 1
    end
end
sleep 0.5

begin
    wtype "123554356345"
    sleep 0.50
    echo "Done"
end

sleep 0.3
sudo pkill ydotoold 2>/dev/null
sudo rm -f /tmp/.ydotool_socket
