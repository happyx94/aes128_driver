APP = aestest

# Add any other object files to this list below
APP_OBJS = aestest.o

COMMON_DIR = ~/projects/common
APP_OBJS += $(COMMON_DIR)/dma_driver.o
HEADERS = $(COMMON_DIR)/dma_driver.h

all: build

build: header $(APP)

header:
	cp $(HEADERS) $(shell pwd)

$(APP): $(APP_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(APP_OBJS) $(LDLIBS)

clean:
	rm -f $(APP_OBJS) $(APP) *.o


