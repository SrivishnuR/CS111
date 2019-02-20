default:
	gcc -Wall -Wextra -o lab4b lab4b.c -lm -lmraa

check: default
	chmod +rx smoketest.sh
	./smoketest.sh

clean:
	rm -f lab4b-504845437.tar.gz lab4b

dist: clean default
	tar -czvf lab4b-504845437.tar.gz lab4b.c Makefile README smoketest.sh
