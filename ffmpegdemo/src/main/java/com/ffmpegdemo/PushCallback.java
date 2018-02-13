package com.ffmpegdemo;

public interface PushCallback {
    public void videoCallback(long pts, long dts, long duration, long index);
}