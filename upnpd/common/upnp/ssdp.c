
#include <netdb.h>
#include <sys/socket.h>

typedef struct ssdp_s ssdp_t;

struct ssdp_s {
	int read_socket;
	int send_socket;
};

static const char *ssdp_ip = "239.255.255.250";
static const unsigned short ssdp_port = 1900;

ssdp_t * ssdp_init (void)
{
	return 0;
}

int sspd_uninit (ssdp_t *ssdp)
{
	return 0;
}
