default: 
	gcc -o lab1b-client lab1b-client.c -Wall -Wextra -lmcrypt	
	gcc -o lab1b-server lab1b-server.c -Wall -Wextra -lmcrypt

client: 
	gcc -o lab1b-client lab1b-client.c -Wall -Wextra -lmcrypt	

server:
	gcc -o lab1b-server lab1b-server.c -Wall -Wextra -lmcrypt

clean:
	rm -f lab1b-client lab1b-server *.tar.gz

dist:
	tar -czvf lab1b-504845437.tar.gz Makefile lab1b-client.c lab1b-server.c my.key README
