/**
 * Picoflexbot is a very small C based IRC bot that can be used (almost) immediately
 * to log conversations on a channel and bounce them back by private-messaging the bot
 * with how many lines you want to be read back to you.  Useful if you want to find out
 * what happened while you were away.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <netdb.h>

#define NETBUF_SIZE 512
#define BACKBUFFER_SIZE 100		// how many lines of conversation to keep (this memory is permanently used!)
#define BACKBUFFER_STR_SIZE 1024	// how big each line can be (1K is more than ample in most cases)

int conn;
char sbuf[NETBUF_SIZE];

void raw(char *fmt, ...) {
	va_list ap;
	int wr;
	va_start(ap, fmt);
	vsnprintf(sbuf, NETBUF_SIZE, fmt, ap);
	va_end(ap);
	printf("<< %s", sbuf);
	wr = write(conn, sbuf, strlen(sbuf));
	if (wr == 0) fprintf(stdout,"Attempted to write %ld bytes, only sent 0\n", strlen(sbuf));
}

int main( int argc, char **argv ) {

	char chanbuf[BACKBUFFER_SIZE][BACKBUFFER_STR_SIZE];
	int si, ei, bc; // rolling buffer parameters
	FILE *f;

	/** You need to customise this portion of the code for your bot to work properly
	 * 
	 */
	char *nick = "name of your bot";
	char *channel = "#channel to join";
	char *host = "server to join";
	char *port = "6667";

	char *user, *command, *where, *message, *sep, *target;
	int i, j, l, sl, o = -1, start, wordcount;
	char buf[NETBUF_SIZE +1];
	struct addrinfo hints, *res;

	si = ei = bc = 0;
	start = 0;

	/** Set the log output file
	*/
	f = fopen("picoflexbot.log","a");
	if (!f) {
		fprintf(stderr,"Can't open log file (%s)\n", strerror(errno));
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	getaddrinfo(host, port, &hints, &res);
	conn = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	connect(conn, res->ai_addr, res->ai_addrlen);

	raw("USER %s 0 0 :%s\r\n", nick, nick);
	raw("NICK %s\r\n", nick);

	while ((sl = read(conn, sbuf, NETBUF_SIZE))) {
		for (i = 0; i < sl; i++) {
			o++;
			buf[o] = sbuf[i];
			if ((i > 0 && sbuf[i] == '\n' && sbuf[i - 1] == '\r') || o == NETBUF_SIZE) {
				buf[o + 1] = '\0';
				l = o;
				o = -1;

				printf(">> %s", buf);

				if (!strncmp(buf, "PING", 4)) {
					buf[1] = 'O';
					raw(buf);
				} else if (buf[0] == ':') {
					wordcount = 0;
					user = command = where = message = NULL;
					for (j = 1; j < l; j++) {
						if (buf[j] == ' ') {
							buf[j] = '\0';
							wordcount++;
							switch(wordcount) {
								case 1: user = buf + 1; break;
								case 2: command = buf + start; break;
								case 3: where = buf + start; break;
							}
							if (j == l - 1) continue;
							start = j + 1;
						} else if (buf[j] == ':' && wordcount == 3) {
							if (j < l - 1) message = buf + j + 1;
							break;
						}
					}

					if (wordcount < 2) continue;

					if (!strncmp(command, "001", 3) && channel != NULL) {
						raw("JOIN %s\r\n", channel);
					} else if (!strncmp(command, "PRIVMSG", 7) || !strncmp(command, "NOTICE", 6)) {
						if (where == NULL || message == NULL) continue;
						if ((sep = strchr(user, '!')) != NULL) user[sep - user] = '\0';
						if (where[0] == '#' || where[0] == '&' || where[0] == '+' || where[0] == '!') {
							// Channel chat
							target = where;
							fprintf(f,"%s: %s\n", user, message);
							snprintf(chanbuf[ei], BACKBUFFER_STR_SIZE -1, "%s: %s\r\n", user, message);
							fflush(f);
							ei++; bc++;
							if (bc >= BACKBUFFER_SIZE) {
								si++;
								bc = BACKBUFFER_SIZE;
							}
							ei = ei%BACKBUFFER_SIZE;
							si = si%BACKBUFFER_SIZE;

						} // Channel chat
						else {
							// Private chat
							char prefix[BACKBUFFER_STR_SIZE];
							int linecount;
							int ci;
							target = user;
							snprintf(prefix,sizeof(prefix),"PRIVMSG %s :", target);
							linecount = atoi(message);
							if (linecount > bc) linecount = bc;


							if (linecount > ei) ci = ei -linecount +BACKBUFFER_SIZE; else ci = ei -linecount;
							while (linecount--) {
								int oi = ci%BACKBUFFER_SIZE;
								ci++;
								if (strlen(chanbuf[oi]) == 0) continue;
								raw(prefix);
								raw(chanbuf[oi]);
								sleep(1);
							} // while

						} // private chat
					} // message or notice data
				} // : prefix data
			}
		} // for each char in the net rx data
	} // while we have a connection...
	return 0;
}
