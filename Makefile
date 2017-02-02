default:
	gcc -pg -pthread -o lab2c lab2c.c SortedList.c
clean:
	$(RM) lab2c *.o *~ *.out
dist:
	tar -cvzf lab2c-404428077.tar.gz lab2c.c SortedList.c SortedList.h Makefile README correctedproj2c.png
