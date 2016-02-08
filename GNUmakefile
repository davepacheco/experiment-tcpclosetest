#
# Makefile for tcpclosetest
#

CLEANFILES	 = tcpclosetest
CFLAGS		+= -Werror -Wall -Wextra -fno-omit-frame-pointer
LDFLAGS		+= -lgen -lsocket -lnsl

tcpclosetest: tcpclosetest.c
	$(CC) -o $@ $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^

clean:
	rm -f $(CLEANFILES)
