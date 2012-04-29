all: cpbbs

cpbbs: cpbbs.c
	gcc cpbbs.c -o cpbbs -lcrypt

clean: 
	rm cpbbs

