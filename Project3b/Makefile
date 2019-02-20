default:
	echo '#!/bin/sh' > lab3b
	echo 'python3 lab3b.py $$1' >> lab3b
	chmod +x lab3b
	
clean:
	rm -f lab3b-504845437.tar.gz lab3b

dist:
	tar -czvf lab3b-504845437.tar.gz lab3b.py Makefile README

