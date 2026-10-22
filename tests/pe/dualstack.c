/*
 * Dual-stack (IPv4-mapped) IPv6 sockets.
 *
 * Windows lets an AF_INET6 socket turn IPV6_V6ONLY off and then reach IPv4
 * peers through ::ffff:a.b.c.d addresses (.NET 5+ creates such sockets by
 * default).  OpenBSD has no IPv4-mapped addresses; wineserver emulates
 * dual-stack TCP clients.  Checks: V6ONLY can be turned off and reads back as
 * 0, a blocking and a non-blocking connect to an IPv4 listener through the
 * mapped address work, the addresses read back as IPv4-mapped IPv6, data
 * flows both ways.  Dual-stack UDP is not emulated: turning V6ONLY off on a
 * datagram socket is expected to fail (WSAEINVAL).  TAP output; the exit
 * status is the number of failed checks.
 *
 *   x86_64-w64-mingw32-gcc -O2 -Wall -o dualstack.exe dualstack.c -lws2_32
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>

static int checks, failures;

static void ok(int cond, const char *what)
{
	checks++;
	if (!cond)
		failures++;
	printf("%s %d - %s\n", cond ? "ok" : "not ok", checks, what);
}

static SOCKET listener(unsigned short *port)
{
	struct sockaddr_in sin;
	int len = sizeof(sin);
	SOCKET l = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(l, (struct sockaddr *)&sin, sizeof(sin)) || listen(l, 4) ||
	    getsockname(l, (struct sockaddr *)&sin, &len))
		return INVALID_SOCKET;
	*port = sin.sin_port;
	return l;
}

static void mapped_addr(struct sockaddr_in6 *sin6, unsigned short port)
{
	memset(sin6, 0, sizeof(*sin6));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = port;
	sin6->sin6_addr.s6_addr[10] = 0xff;
	sin6->sin6_addr.s6_addr[11] = 0xff;
	sin6->sin6_addr.s6_addr[12] = 127;
	sin6->sin6_addr.s6_addr[15] = 1;
}

static int is_mapped_loopback(const struct sockaddr_in6 *sin6)
{
	static const unsigned char v4mapped_lo[16] =
	    { 0,0,0,0, 0,0,0,0, 0,0,0xff,0xff, 127,0,0,1 };
	return sin6->sin6_family == AF_INET6 &&
	    !memcmp(&sin6->sin6_addr, v4mapped_lo, 16);
}

static void dual_tcp(int nonblocking)
{
	struct sockaddr_in6 to, name;
	unsigned short port;
	SOCKET l = listener(&port), s, a;
	DWORD v6only = 1;
	int len = sizeof(v6only), ret;
	char buf[8];
	const char *mode = nonblocking ? "non-blocking" : "blocking";
	char what[128];

	if (l == INVALID_SOCKET) {
		ok(0, "IPv4 listener");
		return;
	}
	s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	v6only = 0;
	ret = setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&v6only, sizeof(v6only));
	snprintf(what, sizeof(what), "%s: IPV6_V6ONLY turned off (error %d)", mode, ret ? WSAGetLastError() : 0);
	ok(!ret, what);
	v6only = 1;
	ret = getsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&v6only, &len);
	snprintf(what, sizeof(what), "%s: IPV6_V6ONLY reads back 0", mode);
	ok(!ret && !v6only, what);

	mapped_addr(&to, port);
	if (nonblocking) {
		u_long nb = 1;
		fd_set wfds;
		struct timeval tv = { 5, 0 };

		ioctlsocket(s, FIONBIO, &nb);
		ret = connect(s, (struct sockaddr *)&to, sizeof(to));
		ok(!ret || WSAGetLastError() == WSAEWOULDBLOCK, "non-blocking: connect started");
		FD_ZERO(&wfds);
		FD_SET(s, &wfds);
		ok(select(0, NULL, &wfds, NULL, &tv) == 1, "non-blocking: connect completed");
		nb = 0;
		ioctlsocket(s, FIONBIO, &nb);
	} else {
		ret = connect(s, (struct sockaddr *)&to, sizeof(to));
		snprintf(what, sizeof(what), "blocking: connect to ::ffff:127.0.0.1 (error %d)", ret ? WSAGetLastError() : 0);
		ok(!ret, what);
	}
	a = accept(l, NULL, NULL);
	ok(a != INVALID_SOCKET, "IPv4 listener accepted the connection");

	len = sizeof(name);
	ret = getpeername(s, (struct sockaddr *)&name, &len);
	snprintf(what, sizeof(what), "%s: getpeername is ::ffff:127.0.0.1", mode);
	ok(!ret && is_mapped_loopback(&name) && name.sin6_port == port, what);
	len = sizeof(name);
	ret = getsockname(s, (struct sockaddr *)&name, &len);
	snprintf(what, sizeof(what), "%s: getsockname is IPv4-mapped", mode);
	ok(!ret && is_mapped_loopback(&name) && name.sin6_port, what);

	ok(send(s, "ping", 4, 0) == 4 && recv(a, buf, sizeof(buf), 0) == 4 &&
	    !memcmp(buf, "ping", 4), "data from the dual-stack socket");
	ok(send(a, "pong", 4, 0) == 4 && recv(s, buf, sizeof(buf), 0) == 4 &&
	    !memcmp(buf, "pong", 4), "data to the dual-stack socket");
	closesocket(a);
	closesocket(s);
	closesocket(l);
}

int main(void)
{
	WSADATA wsa;
	SOCKET u;
	DWORD v6only = 0;
	int ret;

	setvbuf(stdout, NULL, _IONBF, 0);
	WSAStartup(MAKEWORD(2, 2), &wsa);
	dual_tcp(0);
	dual_tcp(1);

	/* the documented limit of the emulation */
	u = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	ret = setsockopt(u, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&v6only, sizeof(v6only));
	printf("# dual-stack UDP: setsockopt %d, error %d (not emulated)\n", ret, ret ? WSAGetLastError() : 0);
	closesocket(u);

	printf("1..%d\n", checks);
	WSACleanup();
	return failures;
}
