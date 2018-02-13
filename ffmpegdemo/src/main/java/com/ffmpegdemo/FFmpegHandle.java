package com.ffmpegdemo;

/**
 * Created by Administrator on 2018-2-11.
 */

public class FFmpegHandle {


    static {
        System.loadLibrary("native-lib");
    }


    private static FFmpegHandle mInstance;


    private FFmpegHandle() {
    }


    public static FFmpegHandle getInstance() {
        if (mInstance == null) {
            mInstance = new FFmpegHandle();
        }
        return mInstance;
    }


    public native int setCallback(PushCallback pushCallback);

    public native String getAvcodecConfiguration();

    public native int pushRtmpFile(String path);


}
