main:
	gcc -Wall -Wextra brightness-control.c $(shell pkg-config --cflags --libs dbus-1) $(shell pkg-config --libs dbus-1) -o brightness-control

