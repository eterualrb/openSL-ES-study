package wh.opensles_study;

public class AudioPlayer {

    static {
        System.loadLibrary("audio-play");
    }

    public native static void play(String inPath);
}
