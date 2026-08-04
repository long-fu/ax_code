CUR_PATH		   := $(shell pwd)
HOME_PATH		   := $(CUR_PATH)/../..
ROOTFS_TARGET_PATH ?= $(HOME_PATH)/rootfs/rootfs/usr/local

include $(HOME_PATH)/build/color.mk
include $(HOME_PATH)/build/config.mak

all:
	@$(ECHO) -e $(GREEN)"\nBuild ffmpeg success!!\n"  $(DONE)


install:
	@$(MKDIR) $(ROOTFS_TARGET_PATH)/lib
	@$(CP) -d $(CUR_PATH)/lib/libavcodec.so* $(ROOTFS_TARGET_PATH)/lib
	@$(CP) -d $(CUR_PATH)/lib/libavfilter.so* $(ROOTFS_TARGET_PATH)/lib
	@$(CP) -d $(CUR_PATH)/lib/libavformat.so* $(ROOTFS_TARGET_PATH)/lib
	@$(CP) -d $(CUR_PATH)/lib/libavutil.so* $(ROOTFS_TARGET_PATH)/lib
	@$(CP) -d $(CUR_PATH)/lib/libswresample.so* $(ROOTFS_TARGET_PATH)/lib
	@$(CP) -d $(CUR_PATH)/lib/libswscale.so* $(ROOTFS_TARGET_PATH)/lib
	@$(ECHO) -e $(GREEN)"\nInstall ffmpeg success!!\n"  $(DONE)

clean:
	@$(RM)  $(ROOTFS_TARGET_PATH)/lib/libavcodec.so*
	@$(RM)  $(ROOTFS_TARGET_PATH)/lib/libavfilter.so*
	@$(RM)  $(ROOTFS_TARGET_PATH)/lib/libavformat.so*
	@$(RM)  $(ROOTFS_TARGET_PATH)/lib/libavutil.so*
	@$(RM)  $(ROOTFS_TARGET_PATH)/lib/libswresample.so*
	@$(RM)  $(ROOTFS_TARGET_PATH)/lib/libswscale.so*

	@$(ECHO) -e $(GREEN)"\nClean ffmpeg success!!\n"  $(DONE)
