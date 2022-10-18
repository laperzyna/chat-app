PROGS = project05

all: $(PROGS)

project05: project05.c
	gcc -g -o project05 project05.c

clean:
	rm -rf $(PROGS)