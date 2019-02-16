DESTDIR ?= /usr/local
DEBUG ?= 0

define find-pkg
	CFLAGS += $(shell pkg-config --cflags $(1))
	LDFLAGS += $(shell pkg-config --libs $(1))
endef

$(eval $(call find-pkg,fuse3 libcurl libxml-2.0))

CFLAGS += -fms-extensions -Icommon
# Clang:  -Wno-microsoft

ifeq ($(DEBUG), 1)
	CFLAGS += -g
else
	CFLAGS += -Os -flto
	LDFLAGS += -s
endif
CFLAGS += -Wall -fPIC -fdata-sections -ffunction-sections
LDFLAGS += -rdynamic -fPIE -Wl,--gc-sections


.PHONY: all
all: networkfs

.PHONY: clean
clean:
	rm -rf networkfs \
		*.o */*.o */*/*.o \
		*.d */*.d */*/*.d

.PHONY: install
install: networkfs
	install -d $(DESTDIR)/bin || true
	install -m 0755 networkfs $(DESTDIR)/bin
	-ln -s networkfs $(DESTDIR)/bin/mount.networkfs

.PHONY: uninstall
uninstall:
	rm -rf $(DESTDIR)/bin/networkfs $(DESTDIR)/bin/mount.networkfs

.PHONY: reinstall
reinstall: uninstall install

OBJS := networkfs.o \
	common/grammar/exception.o common/grammar/malloc.o common/grammar/vtable.o \
	common/template/buffer.o common/template/simple_string.o common/template/stack.o \
	common/wrapper/curl.o \
	common/crc32.o common/utils.o \
	emulator/emulator.o \
	proto/proto.o \
	proto/dav/dav.o proto/dav/method.o proto/dav/parser.o \
	proto/dummy/dummy.o

networkfs: $(OBJS)
