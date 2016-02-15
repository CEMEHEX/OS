all:hello	
hello.o: hello.c
	gcc -c hello.c -o hello.o
hello: hello.o
	gcc -s hello.o -o hello
clean:
	rm hello.o
