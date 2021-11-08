OBJS=pngview.o
BIN=pngview
BIN2=onscreendisplay
OBJS2=onscreendisplay.o

CFLAGS+=-Wall -g -O3 -I../common $(shell libpng-config --cflags)
LDFLAGS+=-L/opt/vc/lib/ -lbcm_host -lm $(shell libpng-config --ldflags) -L../lib -lraspidmx -lraspidmxPng -lpthread

INCLUDES+=-I/opt/vc/include/ -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux

all: $(BIN) $(BIN2)

%.o: %.c
	@rm -f $@ 
	$(CC) $(CFLAGS) $(INCLUDES) -g -c $< -o $@ -Wno-deprecated-declarations

$(BIN): $(OBJS)
	$(CC) -o $@ -Wl,--whole-archive $(OBJS) $(LDFLAGS) -Wl,--no-whole-archive -rdynamic

$(BIN2): $(OBJS2)
	$(CC) -o $@ -Wl,--whole-archive $(OBJS2) $(LDFLAGS) -Wl,--no-whole-archive -rdynamic

clean:
	@rm -f $(OBJS)
	@rm -f $(BIN)
	@rm -f $(OBJS2)
	@rm -f $(BIN2)
