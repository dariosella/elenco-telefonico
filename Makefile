all: server client

server: server.o helper.o user.o contact.o
	gcc -pthread server.o helper.o user.o contact.o -o server

client: client.o helper.o
	gcc -pthread client.o helper.o -o client

server.o: server.c
	gcc -pthread -c server.c

client.o: client.c
	gcc -pthread -c client.c

helper.o: helper.c
	gcc -pthread -c helper.c

user.o: user.c
	gcc -pthread -c user.c

contact.o: contact.c
	gcc -pthread -c contact.c

clean:
	rm -f *.o
	rm -f server
	rm -f client
