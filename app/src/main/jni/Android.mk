LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := connecttoserver
LOCAL_SRC_FILES := main.c
LOCAL_LDLIBS    :=  -llog -lm

include $(BUILD_SHARED_LIBRARY)
LOCAL_LDLIBS+= -L$(SYSROOT)/usr/lib -llog