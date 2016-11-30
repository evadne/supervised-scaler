CC = gcc
CFLAGS = -std=c99 -g -Wall `pkg-config --cflags vips`
LDFLAGS = -lm `pkg-config --libs --cflags vips`
HEADER_FILES = c_src
SOURCE_FILES = c_src/convert.c
OBJECT_FILES = $(SOURCE_FILES:.c=.o)
EXECUTABLE_DIRECTORY = priv_dir
EXECUTABLE = $(EXECUTABLE_DIRECTORY)/convert

all: $(SOURCE_FILES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECT_FILES) $(EXECUTABLE_DIRECTORY)
	$(CC) $(OBJECT_FILES) -o $@ $(LDFLAGS)

$(EXECUTABLE_DIRECTORY):
	mkdir -p $(EXECUTABLE_DIRECTORY)

.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(EXECUTABLE) $(OBJECT_FILES) $(BEAM_FILES)
