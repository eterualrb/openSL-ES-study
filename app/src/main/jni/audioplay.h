#ifndef OPENSLES_STUDY_AUDIOPLAY_H
#define OPENSLES_STUDY_AUDIOPLAY_H

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL
Java_wh_opensles_1study_AudioPlayer_play(JNIEnv *, jclass, jstring);

#ifdef __cplusplus
}
#endif

#endif //OPENSLES_STUDY_AUDIOPLAY_H
