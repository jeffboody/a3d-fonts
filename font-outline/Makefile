TARGET   = font2outline
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
CFLAGS   = $(OPT) -I. -I/usr/include/freetype2
LDFLAGS  = -Ltexgz -ltexgz -Llibcc -lcc -lm -ljpeg -lz -lfreetype
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) libcc texgz
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: libcc texgz

libcc:
	$(MAKE) -C libcc -f Makefile

texgz:
	$(MAKE) -C texgz

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C libcc -f Makefile clean
	$(MAKE) -C texgz clean
	rm libcc
	rm texgz

$(OBJECTS): $(HFILES)
