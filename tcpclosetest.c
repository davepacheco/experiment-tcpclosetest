/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Copyright (c) 2016, Joyent, Inc.
 */

/*
 * tcpclosetest: test program behavior when a TCP connection is closed (with
 * close(2)) and the other side tries to write.  Much of this code is ripped
 * from https://github.com/davepacheco/tcpkatest.
 */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

static const char *ct_arg0;
static int ct_port = 20316;

static void usage(void);
static int do_server(int);
static int do_client(int, const char *);
static int make_tcp_socket();
static void on_sigpipe(int, siginfo_t *, void *);

int
main(int argc, char *argv[])
{
	int err;

	ct_arg0 = argv[0];

	if (argc != 2) {
		usage();
	}

	if (strcmp(argv[1], "server") == 0) {
		err = do_server(ct_port);
	} else {
		err = do_client(ct_port, argv[1]);
	}

	return (err);
}

static void
usage(void)
{
	printf("usage: %s \"server\" | IP_ADDRESS\n", ct_arg0);
	exit(2);
}

/*
 * Prints a timestamp to stdout (with no newline).  This is a prelude to
 * printing some other message.
 */
static void
log_time(void)
{
	int errsave;
	time_t nowt;
	struct tm nowtm;
	char buf[sizeof ("2014-01-01T01:00:00Z")];

	errsave = errno;
	time(&nowt);
	gmtime_r(&nowt, &nowtm);
	if (strftime(buf, sizeof (buf), "%FT%TZ", &nowtm) == 0) {
		err(1, "strftime failed unexpectedly");
	}

	(void) fprintf(stdout, "%s: ", buf);
	errno = errsave;
}

static int
do_server(int port)
{
	int sockfd, clientfd;
	struct sockaddr_in addr;
	struct sockaddr_in client;
	int clientlen = sizeof (client);

	log_time();
	(void) printf("starting as server on port %d\n", ct_port);

	sockfd = make_tcp_socket();
	if (sockfd < 0) {
		return (-1);
	}

	bzero(&addr, sizeof (addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr *)&addr, sizeof (addr)) < 0) {
		warn("bind");
		(void) close(sockfd);
		return (-1);
	}

	if (listen(sockfd, 128) < 0) {
		warn("listen");
		(void) close(sockfd);
		return (-1);
	}

	if ((clientfd = accept(sockfd, (struct sockaddr *)&client,
	    &clientlen)) < 0) {
		warn("accept");
		(void) close(sockfd);
		return (-1);
	}

	log_time();
	(void) printf("accepted connection\n");
	(void) usleep(5000000);

	log_time();
	(void) printf("closing client connection\n");
	(void) close(clientfd);
	(void) usleep(5000000);

	log_time();
	(void) printf("teardown\n");
	(void) close(sockfd);
	return (0);
}

static int
make_tcp_socket(void)
{
	struct protoent *protop;
	int sockfd;

	protop = getprotobyname("tcp");
	if (protop == NULL) {
		warnx("protocol not found: \"tcp\"");
		return (-1);
	}

	sockfd = socket(PF_INET, SOCK_STREAM, protop->p_proto);
	if (sockfd < 0) {
		warn("socket");
	}

	return (sockfd);
}

/*
 * Parse the given IP address and store the result into "addrp".  Returns -1 on
 * bad input.
 */
static int
parse_ipv4(const char *ip, struct sockaddr_in *addrp)
{
	char buf[INET_ADDRSTRLEN];

	(void) strlcpy(buf, ip, sizeof (buf));
	bzero(addrp, sizeof (*addrp));
	if (inet_pton(AF_INET, buf, &addrp->sin_addr) != 1) {
		return (-1);
	}

	addrp->sin_family = AF_INET;
	return (0);
}

static int
do_client(int port, const char *ipaddrz)
{
	int sockfd;
	struct sockaddr_in addr;
	struct sigaction sigact;
	int i, nwritten;
	char databuf[512];

	bzero(databuf, sizeof (databuf));

	if (parse_ipv4(ipaddrz, &addr) != 0) {
		warnx("failed to parse IP address: %s\n", ipaddrz);
		return (-1);
	}
	addr.sin_port = htons(port);

	bzero(&sigact, sizeof (sigact));
	sigact.sa_sigaction = on_sigpipe;
	if (sigaction(SIGPIPE, &sigact, NULL) != 0) {
		warn("sigaction");
		return (-1);
	}

	log_time();
	(void) printf("connecting to %s port %d\n", ipaddrz, port);

	sockfd = make_tcp_socket();
	if (sockfd < 0) {
		return (-1);
	}

	if (connect(sockfd, (struct sockaddr *)&addr, sizeof (addr)) != 0) {
		warn("connect");
		(void) close(sockfd);
		return (-1);
	}

	log_time();
	(void) printf("connected\n");

	for (i = 0; i < 20; i++) {
		log_time();
		(void) printf("write (%d)\n", i);
		nwritten = write(sockfd, databuf, sizeof (databuf));
		if (nwritten != sizeof (databuf)) {
			log_time();
			if (nwritten < 0) {
				(void) printf(
				    "write returned %d (error %d: %s)\n",
				    nwritten, errno, strerror(errno));
			} else {
				(void) printf("write returned %d\n",
				    nwritten);
			}

			break;
		}

		usleep(500000);
	}

	log_time();
	(void) printf("teardown\n");
	(void) close(sockfd);
	return (0);
}

static void
on_sigpipe(int sig, siginfo_t *siginfo __attribute__((unused)),
    void *uctx __attribute__((unused)))
{
	assert(sig == SIGPIPE);
	(void) write(STDOUT_FILENO, "got SIGPIPE\n",
	    sizeof ("got SIGPIPE\n") - 1);
}
