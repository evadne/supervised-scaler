CC= gcc
CFLAGS = -g -Wall `pkg-config --cflags --libs vips`
LDFLAGS = `pkg-config --libs --cflags vips`
HEADER_FILES = c_src
SOURCE_FILES = c_src/convert.c

OBJECT_FILES = $(SOURCE_FILES:.c=.o)

priv_dir/convert: clean priv_dir $(OBJECT_FILES)
	mkdir -p priv_dir
	$(CC) -I $(HEADER_FILES) -o $@ $(LDFLAGS) $(OBJECT_FILES) $(LDLIBS)

priv_dir:
	mkdir -p priv_dir

clean:
	rm -f priv_dir/convert $(OBJECT_FILES) $(BEAM_FILES)
