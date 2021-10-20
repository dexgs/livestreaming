THIRDPARTY_DIR = ./thirdparty

srt_static = $(THIRDPARTY_DIR)/srt/build/libsrt.a

LDFLAGS = $(srt_static) -lpthread -lcrypto
CFLAGS = -Wall -Wextra

.DEFAULT_GOAL := ShaRT

all: srt picohttpparser ShaRT

src = $(wildcard src/*.c) \
	  thirdparty/picohttpparser/picohttpparser.c
obj = $(src:.c=.o)

ShaRT: $(obj)
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
	
	@mkdir -p src/$@
	cp $(THIRDPARTY_DIR)/$@/srtcore/*.h src/$@/
	cp $(THIRDPARTY_DIR)/$@/_build/*.h src/$@/

.PHONY: picohttpparser
picohttpparser:
	@mkdir -p src/$@
	cp $(THIRDPARTY_DIR)/$@/*.h src/$@/

.PHONY: clean
clean:
	rm -f $(obj) bin/ShaRT
	rm -rf $(THIRDPARTY_DIR)/srt/_build
