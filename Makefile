pulserate: pulserate-user.c pulserate.p
	pasm pulserate.p
	gcc -o pulserate -I/usr/local/include/ pulserate-user.c -lprussdrv -lpthread

run: pulserate
	LD_LIBRARY_PATH=/usr/local/lib ./pulserate
