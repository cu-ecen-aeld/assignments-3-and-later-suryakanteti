#! /bin/sh

case "$1" in
    start)
        echo "Starting AESD Socket"
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
        /usr/bin/aesdchar_load
        ;;
    stop)
        echo "Stopping AESD Socket"
        start-stop-daemon -K -n aesdsocket
        /usr/bin/aesdchar_unload
        ;;
    *)
        echo "Usage: $0 {start|stop}"
    exit 1
esac

exit 0
