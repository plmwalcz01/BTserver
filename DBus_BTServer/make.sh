g++ main.c -I/usr/include/dbus-1.0 \
           -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include \
           -I/usr/include/glib-2.0 \
           -I/usr/lib/x86_64-linux-gnu/glib-2.0/include/ \
	   -I/usr/include/gtk-2.0 \
	   -I/usr/lib/x86_64-linux-gnu/gtk-2.0/include \
	   -I/usr/include/ \
	   `pkg-config --cflags --libs gtk+-2.0 obexftp` \
	   -o GDBusBT -ldbus-1 -ldbus-glib-1 -lbluetooth 
	   
