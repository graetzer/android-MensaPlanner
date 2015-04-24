#ifndef __MAIN_H__
#define __MAIN_H__

#include <jni.h>
#include <android/log.h>

// underscores are reserved characters in JNI referring to package boundaries.
// stick with camelCase moduleNames, classNames and methodNames

jint Java_com_sdgsystems_examples_android_ndk_ndkmodule_MainNative_callWithArguments(JNIEnv* env, jobject thiz, jstring deviceName, jint width, jint height);

#endif // __MAIN_H__