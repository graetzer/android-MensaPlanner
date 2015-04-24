#include "main.h"

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>

jint Java_com_sdgsystems_examples_android_ndk_ndkmodule_MainNative_callWithArguments(JNIEnv* env, jobject thiz, jstring deviceName, jint width, jint height) {

    const char* dev_name = (*env)->GetStringUTFChars(env, deviceName, 0);
    (*env)->ReleaseStringUTFChars(env, deviceName, dev_name);

    //TODO something awesome

    return 0;
}
// Required for the default JNI implementation
jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    return JNI_VERSION_1_6;
}

