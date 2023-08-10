all : appendMeMore

appendMeMore : appendMeMore.o
	gcc -o appendMeMore appendMeMore.o -lm

appendMeMore.o : appendMeMore.c
	gcc -c appendMeMore.c

clean :
	rm -rf *.o appendMeMore