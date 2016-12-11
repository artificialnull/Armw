all:
	gcc -lX11 armw.c -o Armw

run:
	Xephyr :9 -ac -screen 800x600 &
	sleep 1
	DISPLAY=:9 urxvt &
	DISPLAY=:9 ./Armw

