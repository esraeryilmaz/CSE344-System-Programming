all: BibakBOXClient BibakBOXServer

BibakBOXClient : BibakBOXClient.o
	gcc -o BibakBOXClient BibakBOXClient.o

BibakBOXClient.o : BibakBOXClient.c
	gcc -c BibakBOXClient.c

BibakBOXServer : BibakBOXServer.o
	gcc -o BibakBOXServer BibakBOXServer.o -pthread

BibakBOXServer.o : BibakBOXServer.c
	gcc -c BibakBOXServer.c -pthread

clean : 
	rm -rf *.o BibakBOXClient BibakBOXServer
