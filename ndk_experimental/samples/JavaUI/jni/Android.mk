LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := JavaUINativeActivity
LOCAL_SRC_FILES := JavaUINativeActivity.cpp \
TeapotRenderer.cpp \

LOCAL_C_INCLUDES :=
LOCAL_CFLAGS := 
LOCAL_CPPFLAGS := -std=c++11

LOCAL_LDLIBS    := -llog -landroid -lEGL -lGLESv2 -latomic
LOCAL_STATIC_LIBRARIES := cpufeatures android_native_app_glue ndk_helper jui_helper

ifneq ($(filter %armeabi-v7a,$(TARGET_ARCH_ABI)),)
LOCAL_CFLAGS += -mhard-float -D_NDK_MATH_NO_SOFTFP=1
LOCAL_LDLIBS += -lm_hard
ifeq (,$(filter -fuse-ld=mcld,$(APP_LDFLAGS) $(LOCAL_LDFLAGS)))
LOCAL_LDFLAGS += -Wl,--no-warn-mismatch
endif
endif

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/ndk_helper)
$(call import-module,android/jui_helper)
$(call import-module,android/native_app_glue)
$(call import-module,android/cpufeatures)
