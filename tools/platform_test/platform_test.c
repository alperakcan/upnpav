
#define _GNU_SOURCE
#include <stdlib.h>
#include "platform.h"
#include "database.h"

uint64_t test_insert(database_t *db, const char *class, const char *parent, int nr)
{
	char buf[255];
	sprintf(buf, "data%d", nr);
	return upnpd_database_insert (db, class, parent, buf, buf, nr ,buf, buf, buf, buf);
}

void test_database()
{
	char s[20];

	database_t *db;
	database_entry_t *entry;
	unsigned long long id;
	unsigned long long total;

	db = upnpd_database_init(0);
	upnpd_database_index(db);

	entry = upnpd_database_query_entry(db, "X");

	id = test_insert(db, "object.item.image", "0", 1);
	id = test_insert(db, "object.container.storageFolder", "0", 2);

	sprintf(s, "%s%llu", "0", id);
	entry = upnpd_database_query_entry(db, s);
	upnpd_database_entry_free(entry);

	sprintf(s, "%llu", id);

	test_insert (db, "object.item.image", s, 3);
	test_insert (db, "object.item.video/x", s, 4);
	test_insert (db, "object.item.video/y", s, 5);
	test_insert (db, "object.container.storageFolder", s, 6);
	
	entry = upnpd_database_query_parent(db, s, 2, 3, &total);
	upnpd_database_entry_free(entry);
	entry = upnpd_database_query_search(db, s, 2, 3, &total, "upnp:class derivedfrom \"object.item.video");
	upnpd_database_entry_free(entry);

	upnpd_database_uninit(db, 0);
}

void test_socket()
{
	socket_t *s;
	poll_item_t pitem[1];
	char bufout[11] = "0123456789";
	char bufin[11];
	int res;
	upnpd_interface_getaddr("x");
	s = upnpd_socket_open(SOCKET_TYPE_STREAM, 0);
	pitem[0].item = s;
	res = upnpd_socket_connect(s, "192.168.2.136", 8888, 3);
	pitem[0].events = POLL_EVENT_OUT;
	upnpd_socket_poll(pitem, 1, 3000);
	upnpd_socket_send(s, bufout, 10);
	pitem[0].events = POLL_EVENT_IN;
	upnpd_socket_poll(pitem, 1, 3000);
	upnpd_socket_recv(s, bufin, 5);
	upnpd_socket_recv(s, bufin + 5, 5);
	upnpd_socket_close(s);
}

void test_file()
{
	char buf[100];
	file_stat_t stat;
	dir_entry_t entry;
	poll_event_t event;
	dir_t *d;
	file_t *f;
	f = upnpd_file_open ("c:/inout/tost.txt", 0);
	f = upnpd_file_open ("c:/inout/test.txt", FILE_MODE_READ | FILE_MODE_WRITE);
	upnpd_file_poll (f, POLL_EVENT_IN, &event, 1000);
	upnpd_file_read (f, buf, 10);
	upnpd_file_seek (f, 10, FILE_SEEK_CUR);
	upnpd_file_poll (f, POLL_EVENT_OUT, &event, 1000);
	upnpd_file_write (f, buf, 10);
	upnpd_file_close (f);
	d = upnpd_file_opendir ("c:/inout/");
	upnpd_file_readdir (d, &entry);
	upnpd_file_closedir (d);
//	upnpd_file_match ("txt", "c:/inout/", 0);
	upnpd_file_stat ("c:/inout/test.txt", &stat);
	upnpd_file_access ("c:/inout/test.txt", FILE_MODE_WRITE);
	upnpd_file_match ("*.txt", "blabla.TXT", 0);
}

void test_winplus()
{
	char buf[100];
	char *q;
	size_t n;
	unsigned long long l;
	snprintf(buf, 10, "blabla %d bla", 1000);
	if (asprintf(&q, "blabla %d bla", 1000) > 0)
		free(q);
	q = NULL;
	#if 0
	getdelim(&q, &n, '\n', stdin);
	n = 1;
	getdelim(&q, &n, '\n', stdin);
	free(q);
	#endif
	platform_debug = 1;
	debugf(_DBG, _DBG, "test");
	debugf(_DBG, _DBG, "test1 %d", 1);
	n = upnpd_rand_rand();
	n = upnpd_rand_rand();
	l = upnpd_time_gettimeofday();
	upnpd_time_strftime(buf, 100, TIME_FORMAT_RFC1123, l, 0);
}

int main (int argc, char *argv[])
{
	platform_init();
	test_database();
	platform_uninit();
	return 0;
}
