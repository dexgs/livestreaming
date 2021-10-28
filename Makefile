THIRDPARTY_DIR = ./thirdparty

srt_static = $(THIRDPARTY_DIR)/srt/_build/libsrt.a

LDFLAGS += $(srt_static) -lpthread -lcrypto
CFLAGS += -Wall -Wextra
CFLAGS += -Ithirdparty/srt/srtcore/ -Ithirdparty/srt/_build
CFLAGS += -Ithirdparty/picohttpparser

.DEFAULT_GOAL := ShaRT

all: srt ShaRT

src = $(wildcard src/*.c) \
	  thirdparty/picohttpparser/picohttpparser.c
obj = $(src:.c=.o)

ShaRT: $(obj)
	@mkdir -p bin
	$(CXX) $(CFLAGS) -o bin/$@ $^ $(LDFLAGS)

test: $(filter-out src/main.o, $(obj)) test/test.o
	@mkdir -p bin
	$(CXX) $(CFLAGS) -o bin/$@ $^ $(LDFLAGS)

SRT_CMAKE_ARGS = \
	-DENABLE_STATIC:bool=1 -DENABLE_SHARED:bool=0 -DENABLE_LOGGING:bool=0 \
	-DENABLE_APPS:bool=0 -DCMAKE_BUILD_TYPE=Release
.PHONY: srt
srt:
	cd $(THIRDPARTY_DIR)/$@; \
	mkdir -p _build; \
	cd _build; \
	cmake ../ $(SRT_CMAKE_ARGS); \
	cmake --build ./;

.PHONY: clean
clean:
	rm -f $(obj) test/test.o bin
	rm -rf $(THIRDPARTY_DIR)/srt/_build
