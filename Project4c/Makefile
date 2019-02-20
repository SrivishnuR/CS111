default:
	gcc -Wall -Wextra -o lab4c_tcp lab4c_tcp.c -lm -lmraa
	gcc -Wall -Wextra -o lab4c_tls lab4c_tls.c -lm -lmraa -lssl -lcrypto

clean:
	rm -f lab4c-504845437.tar.gz lab4c_tcp lab4c_tls

dist: clean default
	tar -czvf lab4c-504845437.tar.gz lab4c_tcp.c lab4c_tls.c Makefile README
