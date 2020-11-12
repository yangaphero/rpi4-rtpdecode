BIN=tvs_player
OBJS=unpackrtp.o sps_pps.o base64.o  avqueue.o  vid_decode.o omx.o main_yuv4.o

INCLUDES+=-I/opt/vc/include/ -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux 
INCLUDES+=-I/opt/vc/src/hello_pi/libs/ilclient

LDFLAGS+=-L/opt/vc/lib -lopenmaxil -lbcm_host -lvcos
LDFLAGS+=-L/opt/vc/src/hello_pi/libs/ilclient -lilclient
LDFLAGS+=-lavcodec -lavutil -lavformat -lavresample
LDFLAGS+=-lpthread -lm

CFLAGS+=-Wall -O2 -mfloat-abi=hard -fomit-frame-pointer
CFLAGS+=-g -rdynamic
CFLAGS+=-DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX \
        -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 \
        -U_FORTIFY_SOURCE -DHAVE_LIBOPENMAX=2 -DOMX  -DRASPBERRY_PI -DOMX_SKIP64BIT -DUSE_EXTERNAL_OMX \
        -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM
CFLAGS+=-ftree-vectorize -Wno-psabi


%.o: %.c
	@rm -f $@ 
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ -c $<

$(BIN): $(OBJS)
	$(CC) -s -o $@ $(OBJS) $(LDFLAGS)

clean:
	@rm -f $(OBJS)
	@rm -f $(BIN)
