all : biboServer biboClient

biboServer : biboServer.o
	gcc -o biboServer biboServer.o -std=gnu99 -pthread -lrt

biboServer.o : biboServer.c
	gcc -c biboServer.c -std=gnu99 -pthread -lrt

biboClient : biboClient.o
	gcc -o biboClient biboClient.o

biboClient.o : biboClient.c
	gcc -c biboClient.c

clean : 
	rm -rf *.o *.log biboServer biboClient