#include "com_cyz14_client4over6_MainActivity.h"

JNIEXPORT jstring JNICALL Java_com_cyz14_client4over6_MainActivity_StringFromJNI
  (JNIEnv * env, jobject jobj) {
  return (*env)->NewStringUTF(env, "Hello from JNI");
  }
