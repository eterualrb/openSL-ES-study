#include "audioplay.h"
#include <android/log.h>

extern "C" {
#include "wavlib.h"
}

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#define LOGI(FORMAT, ...) __android_log_print(ANDROID_LOG_INFO,"audio-player",FORMAT,##__VA_ARGS__)
#define LOGE(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,"audio-player",FORMAT,##__VA_ARGS__)

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))

/**
 * 引擎对象
 */
SLObjectItf gEngineObject;

/**
 * 引擎接口
 */
SLEngineItf gEngineInterface;

/**
 * 混音器对象
 */
SLObjectItf gOutputMixObject;

/**
 * 播放器对象
 */
SLObjectItf gPlayerObject;

/**
 * 缓冲器队列接口
 */
SLAndroidSimpleBufferQueueItf gPlayerBufferQueueItf;

/**
 * 	播放接口
 */
SLPlayItf gPlayInterface;

/**
 * WAV handle
 */
WAV gWav;

/**
 * 音频数据缓冲区
 */
unsigned char *gBuffer;

/**
 * 缓冲区大小
 */
size_t gBufferSize;

/**
 * 封装播放上下文
 */
struct PlayerContext {
    WAV wav;
    unsigned char *buffer;
    size_t bufferSize;

    PlayerContext(WAV wav,
                  unsigned char *buffer,
                  size_t bufferSize) {
        this->wav = wav;
        this->buffer = buffer;
        this->bufferSize = bufferSize;
    }
};

/**
 * 打开wav文件
 * @param filename
 * @return
 */
WAV openWavFile(const char *filename) {
    WAVError wavError;
    WAV wav = wav_open(filename, WAV_READ, &wavError);
    if (NULL == wav) {
        LOGE("wav open error : %s\n", wav_strerror(wavError));
        return NULL;
    }

    // test
//    LOGI("bit rate : %d\n", wav_get_bitrate(wav));
//    LOGI("bits : %d\n", wav_get_bits(wav));
//    LOGI("channels : %d\n", wav_get_channels(wav));
//    LOGI("rate : %d\n", wav_get_rate(wav));

    return wav;
}

/**
 * 关闭wav文件
 * @param wav
 */
void closeWaveFile(WAV wav) {
    wav_close(wav);
}

/**
 * 实例化对象
 * @param slObjectItf
 */
void realizeObject(SLObjectItf slObjectItf) {
    // 同步实例化，会造成阻塞
    (*slObjectItf)->Realize(slObjectItf, SL_BOOLEAN_FALSE);
}

/**
 * 创建带有缓冲区队列的音频播放器
 * @param wav
 * @param engineEngine
 * @param outputMixObject
 * @param playerObject
 */
void createBufferQueuePlayer(
        WAV wav,
        SLEngineItf engineEngine,
        SLObjectItf outputMixObject,
        SLObjectItf &playerObject) {
    // ---------- 创建DataSource start ----------
    // Android针对数据源的简单缓冲区队列定位器
    SLDataLocator_AndroidSimpleBufferQueue dataSourceLocator = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, // 定位器类型，注释中要求必须设置为此type
            1 // 缓冲区个数
    };
    // PCM数据源格式
    SLDataFormat_PCM dataSourceFormat = {
            SL_DATAFORMAT_PCM, // 格式类型为PCM
            wav_get_channels(wav), // 通道数
            wav_get_rate(wav) * 1000, // 每秒的样本数
            wav_get_bits(wav), // 每个样本的位数
            wav_get_bits(wav), // 容器大小
            SL_SPEAKER_FRONT_CENTER,  // 通道屏蔽
            SL_BYTEORDER_LITTLEENDIAN // 字节顺序
    };
    // 数据源设置为PCM格式，带有简单缓冲区队列
    SLDataSource dataSource = {
            &dataSourceLocator,
            &dataSourceFormat
    };
    // ---------- 创建DataSource end ----------

    // ---------- 创建DataSink start ----------
    SLDataLocator_OutputMix dataSinkLocator = {
            SL_DATALOCATOR_OUTPUTMIX, // 定位器类型，注释中要求必须设置为此type
            outputMixObject // 混合输出
    };
    SLDataSink dataSink = {
            &dataSinkLocator,
            NULL
    };
    // ---------- 创建DataSink end ----------

    // 需要的接口
    SLInterfaceID interfaceIds[] = {
            SL_IID_BUFFERQUEUE
    };

    // 如果所需要的接口不能用，请求将失败
    SLboolean requiredInterfaces[] = {
            SL_BOOLEAN_TRUE // for SL_IID_BUFFERQUEUE
    };

    // 创建音频播放器对象
    SLresult result = (*engineEngine)->CreateAudioPlayer(
            engineEngine,
            &playerObject,
            &dataSource,
            &dataSink,
            ARRAY_LEN(interfaceIds),
            interfaceIds,
            requiredInterfaces);
}

/**
 * 播放回调函数
 * @param andioPlayerBufferQueue
 * @param context
 */
void playerCallBack(SLAndroidSimpleBufferQueueItf andioPlayerBufferQueue, void *context) {
    PlayerContext *ctx = (PlayerContext *) context;
    //读取数据
    ssize_t readSize = wav_read_data(ctx->wav, ctx->buffer, ctx->bufferSize);
    if (0 < readSize) {
        (*andioPlayerBufferQueue)->Enqueue(andioPlayerBufferQueue, ctx->buffer, readSize);
    } else {
        //destroy context
        closeWaveFile(ctx->wav); //关闭文件
        delete ctx->buffer; //释放缓存
    }
}

JNIEXPORT void JNICALL
Java_wh_opensles_1study_AudioPlayer_play(JNIEnv *env, jclass type, jstring inPath_) {
    const char *inPath = env->GetStringUTFChars(inPath_, 0);
    LOGI("in path : %s\n", inPath);

    // 1. 打开wav文件
    gWav = openWavFile(inPath);

    // 2. 创建OpenSL ES引擎并实例化
    // opensl es在android平台下默认是线程安全的
    // 这样设置是为了兼容其他平台
    SLEngineOption options[] = {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE};
    // 创建引擎
    slCreateEngine(
            &gEngineObject, // opensl es引擎
            ARRAY_LEN(options), // 配置项个数
            options, // 配置项
            0, // 接口个数
            NULL, // 接口id集合
            NULL
    );
    // 实例化引擎
    realizeObject(gEngineObject);

    // 3. 获取引擎接口
    (*gEngineObject)->GetInterface(gEngineObject, SL_IID_ENGINE, &gEngineInterface);

    // 4. 创建输出混音器并实例化
    (*gEngineInterface)->CreateOutputMix(gEngineInterface, &gOutputMixObject, 0, NULL, NULL);
    realizeObject(gOutputMixObject);

    // 5. 创建缓冲区保存读取到的音频数据
    // 缓冲区大小
    gBufferSize = wav_get_rate(gWav) * wav_get_channels(gWav) * wav_get_bits(gWav);
    gBuffer = new unsigned char[gBufferSize];

    // 6. 创建带有缓冲区队列的音频播放器并实例化
    createBufferQueuePlayer(gWav, gEngineInterface, gOutputMixObject, gPlayerObject);
    realizeObject(gPlayerObject);

    // 7. 获得缓冲区队列接口
    // 通过缓冲区队列接口对缓冲区进行顺序播放
    (*gPlayerObject)->GetInterface(gPlayerObject, SL_IID_BUFFERQUEUE, &gPlayerBufferQueueItf);

    // 8. 注册音频播放器回调函数
    // 当播放器完成对前一个缓冲区队列的播放时，回调函数会被调用
    // 然后我们又继续读取音频数据，直到结束
    // 播放上下文，包裹参数方便在回调函数中使用
    PlayerContext *ctx = new PlayerContext(gWav, gBuffer, gBufferSize);
    (*gPlayerBufferQueueItf)->RegisterCallback(
            gPlayerBufferQueueItf,
            playerCallBack,
            ctx
    );

    // 9. 获取Play Interface通过对SetPlayState函数来启动播放音乐
    // 一旦播放器被设置为播放状态，该音频播放器开始等待缓冲区排队就绪
    (*gPlayerObject)->GetInterface(gPlayerObject, SL_IID_PLAY, &gPlayInterface);
    // 设置播放状态
    (*gPlayInterface)->SetPlayState(gPlayInterface, SL_PLAYSTATE_PLAYING);

    // 10. 开始，让第一个缓冲区入队进行播放，然后回调函数会一直调用，知道音频数据读取完毕
    playerCallBack(gPlayerBufferQueueItf, ctx);

    env->ReleaseStringUTFChars(inPath_, inPath);
}