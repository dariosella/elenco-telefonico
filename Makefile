all: server client

server: server.o helper.o user.o contact.o
	gcc server.o helper.o user.o contact.o -o server

client: client.o helper.o
	gcc client.o helper.o -o client

server.o: server.c
	gcc -c server.c

client.o: client.c
	gcc -c client.c

helper.o: helper.c
	gcc -c helper.c

user.o: user.c
	gcc -c user.c

contact.o: contact.c
	gcc -c contact.c

clean:
	rm -f *.o
	rm -f server
	rm -f client
