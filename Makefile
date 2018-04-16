APP = aes128

# Add any other object files to this list below
APP_OBJS = aes128.o

COMMON_DIR = ~/projects/common
APP_OBJS += $(COMMON_DIR)/dma_driver.o
APP_OBJS += $(COMMON_DIR)/sw_aes.o
HEADERS = $(COMMON_DIR)/dma_driver.h $(COMMON_DIR)/sw_aes.h

all: build

build: header $(APP)

header:
	cp $(HEADERS) $(shell pwd)

$(APP): $(APP_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(APP_OBJS) $(LDLIBS)

clean:
	rm -f $(APP_OBJS) $(APP) *.o
	rm -f $(COMMON_DIR)/*.o
