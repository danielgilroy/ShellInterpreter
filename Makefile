.phony all:
all: shell

shell: shell.c
	gcc shell.c -o shell -lreadline -lhistory -ltermcap

.PHONY clean:
clean:
	-rm -rf *.o *.exe shell
