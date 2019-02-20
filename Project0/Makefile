lab0make: lab0.c
	gcc -o lab0 lab0.c -Wextra -Wall

clean:
	rm -f lab0 *.txt *.tar.gz

dist:
	tar -czvf lab0-504845437.tar.gz Makefile lab0.c README backtrace.png breakpoint.png

check:	test1 test2 test3 test4 test5

test1:
	echo "the quick brown fox jumps over the lazy dog" > original.txt;\
	touch redirected.txt;\
	./lab0 --input original.txt --output redirected.txt;\
	diff original.txt redirected.txt;\
	if [ $$? != 0 ]; then \
		echo "Error: Input and Output not identical";\
	fi;\
	rm original.txt redirected.txt;

test2:
	./lab0 --badarg;\
	if [ $$? != 1 ]; then \
		echo "Error: Not catching unrecognized arguments";\
	fi;

test3:
	./lab0 --input badfile.txt;\
	if [ $$? != 2 ]; then \
		echo "Error: Not catching bad input file";\
	fi;

test4:
	touch badfile.txt;\
	chmod 000 badfile.txt;\
	./lab0 --output badfile.txt;\
	if [ $$? != 3 ]; then \
		echo "Error: Not catching bad input file";\
	fi;\
	rm -f badfile.txt;

test5:
	./lab0 --segfault --catch;\
	if [ $$? != 4 ]; then \
		echo "Error: Not catching segmentation fault";\
	fi;
