#include "platform.h"

void test_file()
{
	file_t *f;
	upnpd_file_unlink("c:/inout/test.txt");
	f = upnpd_file_open ("c:/inout/test.txt", FILE_MODE_WRITE | FILE_MODE_CREATE);
	upnpd_file_write (f, "1234567890", 10);
	upnpd_file_close (f);
}

void test_socket()
{
	socket_t *s;
	socket_t *s1;
	poll_item_t pitem[1];
	char bufout[11] = "0123456789";
	char bufin[11];
	s = upnpd_socket_open(SOCKET_TYPE_STREAM, 1);
	upnpd_socket_bind(s, "192.168.2.136", 8888);
	upnpd_socket_listen(s, 1);
	while (!(s1 = upnpd_socket_accept(s)))
		;
	pitem[0].item = s1;
	pitem[0].events = POLL_EVENT_IN;
	upnpd_socket_poll(pitem, 1, 3000);
	upnpd_socket_recv(s1, bufin, 10);
	pitem[0].events = POLL_EVENT_OUT;
	upnpd_socket_poll(pitem, 1, 3000);
	upnpd_socket_send(s1, bufout, 10);
	upnpd_socket_close(s1);
	upnpd_socket_close(s);
}

int main (int argc, char *argv[])
{
	platform_init();
	test_file();
	platform_uninit();
	return 0;
}
