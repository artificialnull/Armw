all:
	gcc -lX11 simple.c -o simplewm

run:
	Xephyr :1 -ac -screen 800x600 &
	sleep 1
	DISPLAY=:1 ./simplewm &
	DISPLAY=:1 urxvt &

