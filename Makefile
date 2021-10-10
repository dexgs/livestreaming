THIRDPARTY_DIR = ./thirdparty

srt_static = $(THIRDPARTY_DIR)/srt/build/libsrt.a
datachannel_static = $(THIRDPARTY_DIR)/libdatachannel/build/libdatachannel-static.a

LDFLAGS = $(srt_static) $(datachannel_static) -lpthread -lssl -lcrypto
CFLAGS = -Wall -Wextra

.DEFAULT_GOAL := ShaRT

all: srt libdatachannel ShaRT

src = $(wildcard src/*.c)
obj = $(src:.c=.o)

ShaRT: $(obj)
	@mkdir -p bin
	$(CXX) $(CFLAGS) -o bin/$@ $^ $(LDFLAGS)


# SRT and libdatachannel are both configured
# such that they can be statically linked.

SRT_CMAKE_ARGS = \
	-DENABLE_STATIC:bool=1 -DENABLE_SHARED:bool=0 -DENABLE_LOGGING:bool=0 \
	-DENABLE_APPS:bool=0 -DCMAKE_BUILD_TYPE=Release
.PHONY: srt
srt:
	cd $(THIRDPARTY_DIR)/$@; \
	mkdir -p build; \
	cd build; \
	cmake ../ $(SRT_CMAKE_ARGS); \
	cmake --build ./;
	
	@mkdir -p src/thirdparty/$@
	cp $(THIRDPARTY_DIR)/$@/srtcore/*.h src/thirdparty/$@/
	cp $(THIRDPARTY_DIR)/$@/build/*.h src/thirdparty/$@/

DATACHANNEL_CMAKE_ARGS = -DCMAKE_BUILD_TYPE=Release
.PHONY: libdatachannel
libdatachannel:
	cd $(THIRDPARTY_DIR)/$@; \
	mkdir -p build; \
	cd build; \
	cmake ../ $(DATACHANNEL_CMAKE_ARGS); \
	cmake --build ./ --target datachannel-static;
	
	@mkdir -p src/thirdparty/$@
	cp $(THIRDPARTY_DIR)/$@/include/rtc/*.h src/thirdparty/$@/


.PHONY: clean
clean:
	rm -f $(obj) ShaRT
	rm -rf $(THIRDPARTY_DIR)/libdatachannel/build
	rm -rf $(THIRDPARTY_DIR)/srt/build
