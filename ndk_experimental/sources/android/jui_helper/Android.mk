LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE:= jui_helper
LOCAL_SRC_FILES:= JavaUI.cpp \
JavaUI_Toast.cpp \
JavaUI_Window.cpp \
JavaUI_Dialog.cpp \
JavaUI_Layouts.cpp

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)
LOCAL_EXPORT_LDLIBS    := -llog -landroid

LOCAL_STATIC_LIBRARIES := ndk_helper

LOCAL_CFLAGS += -std=c++11

ifneq ($(filter %armeabi-v7a,$(TARGET_ARCH_ABI)),)
LOCAL_CFLAGS += -mhard-float -D_NDK_MATH_NO_SOFTFP=1
LOCAL_EXPORT_CFLAGS += -mhard-float -D_NDK_MATH_NO_SOFTFP=1
LOCAL_EXPORT_LDLIBS += -lm_hard
ifeq (,$(filter -fuse-ld=mcld,$(APP_LDFLAGS) $(LOCAL_LDFLAGS)))
LOCAL_EXPORT_LDFLAGS += -Wl,--no-warn-mismatch
endif
endif

include $(BUILD_STATIC_LIBRARY)

$(call import-module,android/ndk_helper)
