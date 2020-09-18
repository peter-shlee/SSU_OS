all : ssu_shell ttop pps

ssu_shell : ssu_shell.o
	gcc ssu_shell.o -o ssu_shell

ttop : ttop.o
	gcc ttop.o -o ttop -lncurses

pps : pps.o
	gcc pps.o -o pps -lncurses

ssu_shell.o : ssu_shell.c
	gcc -c ssu_shell.c

ttop.o : ttop.c
	gcc -c ttop.c

pps.o : pps.c
	gcc -c pps.c
