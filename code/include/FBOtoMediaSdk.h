/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class FBOtoMediaSdk */

#ifndef _Included_FBOtoMediaSdk
#define _Included_FBOtoMediaSdk
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     FBOtoMediaSdk
 * Method:    FBOInit
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_FBOtoMediaSdk_FBOInit
  (JNIEnv *, jobject, jint, jint);

/*
 * Class:     FBOtoMediaSdk
 * Method:    MsdkInit
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_FBOtoMediaSdk_MsdkInit
  (JNIEnv *, jobject, jint, jint);

/*
 * Class:     FBOtoMediaSdk
 * Method:    FBOsetup
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_FBOtoMediaSdk_FBOsetup
  (JNIEnv *, jobject);

/*
 * Class:     FBOtoMediaSdk
 * Method:    cleanUp
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_FBOtoMediaSdk_cleanUp
  (JNIEnv *, jobject);

/*
 * Class:     FBOtoMediaSdk
 * Method:    processFrame
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_FBOtoMediaSdk_processFrame
  (JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif
#endif