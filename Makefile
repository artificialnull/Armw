all:
	gcc -lX11 armw.c -o Armw

run:
	Xephyr :1 -ac -screen 800x600 &
	sleep 1
	DISPLAY=:1 urxvt &
	DISPLAY=:1 ./Armw

