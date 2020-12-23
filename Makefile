# Create your own targets that compile the application
test :
	@echo -e '\n*******************************'
	@echo -e '*** Compiling for TEST ***'
	@echo -e '*******************************'
	clear
	gcc main.c sbuffer.c connmgr.c datamgr.c sensor_db.c lib/tcpsock.c lib/dplist.c -lsqlite3 -pthread  -o main -Wall -std=gnu11 -Werror -DSET_MAX_TEMP=18 -DSET_MIN_TEMP=15 -DTIMEOUT=5 -DPORT=1235 -DDEBUG_PRINT=1
	gcc sensor_node.c lib/tcpsock.c lib/dplist.c -o client
	./main 1234
debug :
	@echo -e '\n*******************************'
	@echo -e '*** Compiling for DEBUG ***'
	@echo -e '*******************************'
	clear
	gcc main.c sbuffer.c connmgr.c datamgr.c sensor_db.c lib/tcpsock.c lib/dplist.c -lsqlite3 -pthread  -o main -Wall -std=gnu11 -Werror -DSET_MAX_TEMP=18 -DSET_MIN_TEMP=15 -DTIMEOUT=5 -DPORT=1234 -g
	gcc sensor_node.c lib/tcpsock.c lib/dplist.c -o client
	gdb ./main
valgrind :
	@echo -e '\n*******************************'
	@echo -e '*** Compiling for VALGRIND ***'
	@echo -e '*******************************'
	clear
	gcc main.c sbuffer.c connmgr.c datamgr.c sensor_db.c lib/tcpsock.c lib/dplist.c -lsqlite3 -pthread  -o main -Wall -std=gnu11 -Werror -DSET_MAX_TEMP=18 -DSET_MIN_TEMP=15 -DTIMEOUT=5 -DPORT=1234 -g
	gcc sensor_node.c lib/tcpsock.c lib/dplist.c -o client
	valgrind --leak-check=full --show-leak-kinds=all -v ./main 1234

labtool :
	@echo -e '\n*******************************'
	@echo -e '*** Compiling for LABTOOL ***'
	@echo -e '*******************************'
	clear
	gcc lib/dplist.c -o lib/libdplist.so -lm -fPIC -shared
	gcc lib/tcpsock.c -o lib/libtcpsock.so -lm -fPIC -shared
	gcc sensor_node.c -o sensor_node -Wall -std=c11 -Werror -DLOOPS=5 -lm -L./lib -Wl,-rpath=./lib -ltcpsock
	gcc main.c connmgr.c datamgr.c sbuffer.c sensor_db.c -o sensor_gateway -Wall -std=c11 -Werror -lm -L./lib -Wl,-rpath=./lib -ltcpsock -ldplist -lpthread -lsqlite3 -DTIMEOUT=5 -DSET_MAX_TEMP=20 -DSET_MIN_TEMP=10
	valgrind --leak-check=full --show-leak-kinds=all -v ./main 1234
gprof :
	@echo -e '\n*******************************'
	@echo -e '*** Compiling for GPROF ***'
	@echo -e '*******************************'
	clear
	gcc main.c sbuffer.c connmgr.c datamgr.c sensor_db.c lib/tcpsock.c lib/dplist.c -pg -no-pie -lsqlite3 -pthread -o main -Wall -std=gnu11 -Werror -DSET_MAX_TEMP=18 -DSET_MIN_TEMP=15 -DTIMEOUT=5 -DPORT=1235 -DDEBUG_PRINT=1
	gcc sensor_node.c lib/tcpsock.c lib/dplist.c -o client 
	./main 1234
	gprof main gmon.out > analysis.txt
gprofvalgrind :
	@echo -e '\n*******************************'
	@echo -e '*** Compiling for GPROFANDVLG ***'
	@echo -e '*******************************'
	clear
	gcc main.c sbuffer.c connmgr.c datamgr.c sensor_db.c lib/tcpsock.c lib/dplist.c -lsqlite3 -pthread -o main -Wall -std=gnu11 -Werror -DSET_MAX_TEMP=18 -DSET_MIN_TEMP=15 -DTIMEOUT=5 -DPORT=1235 -DDEBUG_PRINT=1 -g -pg -no-pie
	gcc sensor_node.c lib/tcpsock.c lib/dplist.c -o client 
	valgrind --leak-check=full --show-leak-kinds=all -v ./main 1234
	gprof main gmon.out > analysis.txt
