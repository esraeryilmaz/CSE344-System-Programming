all : hw2

hw2 : main.o terminalEmulator.o
	gcc -o hw2 main.o terminalEmulator.o

main.o : main.c
	gcc -c main.c -std=gnu99

terminalEmulator.o : terminalEmulator.c
	gcc -c terminalEmulator.c -std=gnu99

clean :
	rm -rf *.o *.log *.txt hw2