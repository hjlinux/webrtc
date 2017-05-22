T8000_PATH := /home/hejl/workspace/Hi3531A_SDK_V2.0.2.0/mpp/T8000

REL_LIB := /home/hejl/workspace/Hi3531A_SDK_V2.0.2.0/mpp/T8000/hi/lib
HI_COMMON := $(T8000_PATH)/src/media/sdk/HiCommon
CFLAGS := -I$(HI_COMMON) \
          -I$(T8000_PATH)/hi/include \
          -I$(T8000_PATH)/lib/ffmpeg-2.8.5/include/ \
          -I$(T8000_PATH)/src/media/message/ \
          -I$(T8000_PATH)/src/media/include/ \

BUILD := arm-hisiv400-linux-

COMM_OBJ := $(HI_COMMON)/t800x-zk_comm_audio.o $(HI_COMMON)/t800x-zk_comm_ivs.o $(HI_COMMON)/t800x-loadbmp.o $(HI_COMMON)/t800x-zk_comm_venc.o \
            $(HI_COMMON)/t800x-zk_comm_vda.o $(HI_COMMON)/t800x-zk_comm_vo.o $(HI_COMMON)/t800x-zk_comm_vpss.o $(HI_COMMON)/t800x-zk_comm_vdec.o \
            $(HI_COMMON)/t800x-zk_comm_sys.o $(HI_COMMON)/t800x-zk_comm_vi.o $(T8000_PATH)/src/media/sdk/base/t800x-zk_vdec.o \
#            ../media/sdk/base/t800x-zk_source_monitor.o

MPI_LIBS := $(REL_LIB)/libmpi.a
MPI_LIBS += $(REL_LIB)/libhdmi.a
ADAPT_DIR := /home/hejl/workspace/webrtc/webrtc-checkout/src/out/Release/obj/_adapt_arm
#ADAPT_DIR := /home/hejl/workspace/webrtc/webrtc-checkout/src/out_arm/Release/obj/_adapt_arm
#C_FLAG = -g
C_FLAG = -O2

all:
	$(BUILD)gcc $(CFLAGS) -c display_arm.c  -o display.o $(C_FLAG)
	$(BUILD)gcc $(CFLAGS) -c h264_dec_arm.c -o h264_dec_arm.o $(C_FLAG)
	$(BUILD)gcc $(CFLAGS) -c h264_enc_arm.c -o h264_enc_arm.o $(C_FLAG)
	$(BUILD)gcc $(CFLAGS) -c capture_arm.c -o capture_arm.o $(C_FLAG)
	$(BUILD)gcc $(CFLAGS) -c resource_note.c -o resource_note.o $(C_FLAG)
	$(BUILD)gcc $(CFLAGS) -c message_queue.c -o message_queue.o $(C_FLAG)
	$(BUILD)gcc $(CFLAGS) -c param_conf.c -o param_conf.o $(C_FLAG)
	$(BUILD)gcc $(CFLAGS) -c audio_pcm.c -o audio_pcm.o $(C_FLAG)
	$(BUILD)gcc $(CFLAGS) -c class_socket.c -o class_socket.o $(C_FLAG)
	$(BUILD)gcc $(CFLAGS) -c class_interface.c -o class_interface.o $(C_FLAG)
	$(BUILD)gcc $(CFLAGS) -I./sqlite/include -c sqlite_interface.c -o sqlite_interface.o $(C_FLAG)

lib:
	$(BUILD)ar rcs libadapt_arm.a *.o

client:
	$(BUILD)gcc $(CFLAGS) -ldl -lpthread test.c *.o $(COMM_OBJ) -o client
master:
	$(BUILD)gcc $(CFLAGS) -ldl -lpthread master.c -o master

copy:
	mkdir -p $(ADAPT_DIR)
	cp libadapt_arm.a $(ADAPT_DIR) 
	cp display_arm.h $(ADAPT_DIR)/display.h
	cp h264_dec_arm.h $(ADAPT_DIR)/h264_dec.h
	cp h264_enc_arm.h $(ADAPT_DIR)/h264_enc.h
	cp capture_arm.h $(ADAPT_DIR)/capture.h
	cp resource_note.h $(ADAPT_DIR)/
	cp param_conf.h $(ADAPT_DIR)/
	cp audio_pcm.h $(ADAPT_DIR)/
	cp message_queue.h $(ADAPT_DIR)/
	cp class_interface.h $(ADAPT_DIR)/
	cp sqlite_interface.h $(ADAPT_DIR)/
	cp sqlite -rf $(ADAPT_DIR)/

clean:
	rm -f *.o *.a master
