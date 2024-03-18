CC=gcc
CFLAGS= -Wall -ggdb

ODIR=obj

_DEPS =mparser.h 
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))
FDEPS =$(_DEPS)

_OBJ = main.o mparser.o  

OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

STYLE=-style="{BasedOnStyle: Microsoft, UseTab: Always,IndentWidth: 4,TabWidth: 4}"
all:wp

 
$(ODIR)/mparser.o: mparser.c $(FDEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/main.o: main.c $(FDEPS)
	mkdir -p obj
	$(CC) -c -o $@ $< $(CFLAGS)

wp: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)


.PHONY: clean
clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 



.PHONY: format_txt 

format_txt: 
	clang-format *.c -i $(STYLE)
	clang-format *.h -i $(STYLE)
 
