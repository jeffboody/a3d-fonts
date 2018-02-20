TARGET   = fontgen
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
#OPT      = -O2 -Wall
OPT      = -g -Wall
CFLAGS   = $(OPT) -I. -I/usr/local/include/freetype2
LDFLAGS  = -La3d -la3d -lm -Ltexgz -ltexgz -lpng -ljpeg -lz -lfreetype
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) a3d texgz
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: a3d texgz

a3d:
	$(MAKE) -C a3d -f Makefile.sdl

texgz:
	$(MAKE) -C texgz

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C a3d -f Makefile.sdl clean
	$(MAKE) -C texgz clean
	rm a3d
	rm texgz

$(OBJECTS): $(HFILES)
