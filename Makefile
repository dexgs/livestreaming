THIRDPARTY_DIR = ./thirdparty
SRT_DIR = $(THIRDPARTY_DIR)/srt
PICOHTTP_DIR = $(THIRDPARTY_DIR)/picohttpparser

INCLUDE_SRT_HEADERS = -I$(SRT_DIR)/srtcore/ -I$(SRT_DIR)/_build
INCLUDE_PICOHTTP_HEADERS = -I$(PICOHTTP_DIR)

SRT_STATIC = $(SRT_DIR)/_build/libsrt.a

CFLAGS += -Wall -Wextra
CFLAGS += $(INCLUDE_SRT_HEADERS)
CFLAGS += $(INCLUDE_PICOHTTP_HEADERS)
LDFLAGS += $(SRT_STATIC) -lpthread -lcrypto

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
	rm -rf $(obj) test/test.o bin
	rm -rf $(THIRDPARTY_DIR)/srt/_build

ifeq ($(PREFIX),)
	PREFIX := /usr
endif

.PHONY: install
install: bin/ShaRT
	install -d $(DESTDIR)$(PREFIX)/bin
	cp $< $(DESTDIR)$(PREFIX)/bin/ShaRT

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/ShaRT
