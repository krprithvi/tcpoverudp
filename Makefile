CC=gcc
OBJF=./obj
CFLAGS=-g

_OBJS=tcpd.o timer.o ftp_client.o ftp_server.o tcpabstraction.o
OBJS=$(patsubst %,$(OBJF)/%,$(_OBJS))


all : ftp_client ftp_server tcpd timer

$(OBJF)/%.o : %.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ -c $<


ftp_client : obj/tcpabstraction.o obj/ftp_client.o
	$(CC) $(CFLAGS) -o $@ $^

ftp_server : obj/tcpabstraction.o obj/ftp_server.o
	$(CC) $(CFLAGS) -o $@ $^

tcpd : obj/tcpd.o
	$(CC) $(CFLAGS) -o $@ $^

timer : obj/timer.o
	$(CC) $(CFLAGS) -o $@ $^

.PHONY : clean

clean:
	rm timer ftp_client ftp_server tcpd -f $(OBJS)
