#! /bin/sh

case "$1" in
    start)
        echo "Starting AESD Socket"
        start-stop-daemon -S -n aesdsocket -a /home/surya/Documents/AESD/Assignment1/assignment-1-suryakanteti/server/aesdsocket -- -d
        ;;
    stop)
        echo "Stopping AESD Socket"
        start-stop-daemon -K -n aesdsocket
        ;;
    *)
        echo "Usage: $0 {start|stop}"
    exit 1
esac

exit 0