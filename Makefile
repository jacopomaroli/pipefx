
CC := gcc
CXX := g++
CFLAGS += -Isrc -Wall -std=gnu99
CXXFLAGS += -Isrc -I/usr/local/include/q -std=c++14 -Wall -Wno-sign-compare \
    -Wno-unused-local-typedefs -Winit-self -rdynamic \
    -DHAVE_POSIX_MEMALIGN
LDLIBS += -ldl -lm -Wl,-Bstatic -Wl,-Bdynamic -lrt -lpthread \
    -lasound


# Set optimization level.
CFLAGS += -O3
CXXFLAGS += -O3


COMMON_OBJ = src/audio.o src/fifo.o src/pa_ringbuffer.o src/util.o src/fxs.o
PIPEFX_OBJ = $(COMMON_OBJ) src/pipefx.o

all: pipefx

debug: CPPFLAGS += -g
debug: all

pipefx: $(PIPEFX_OBJ)
	$(CXX) $(PIPEFX_OBJ) $(LDLIBS) -o pipefx

clean:
	-rm -f src/*.o pipefx
