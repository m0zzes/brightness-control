main:
	gcc -Wall -Wextra acpi-ctl.c -o acpi-ctl 
run:
	gcc -Wall -Wextra acpi-ctl.c -o acpi-ctl 
	./acpi-ctl
test:
	gcc brightness-control.c $(shell pkg-config --cflags --libs dbus-1) $(shell pkg-config --libs dbus-1)

