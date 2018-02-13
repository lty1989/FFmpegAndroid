package com.ffmpegdemo;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;

/**
 * Created by Administrator on 2018-2-11.
 */

public class PushDiskAtivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);


        //  FFmpegHandle.getInstance().pushRtmpFile("/storage/emulated/0/fff.flv");
        new PushThread().start();

    }


    static class PushThread extends Thread {

        @Override
        public void run() {
            FFmpegHandle.getInstance().pushRtmpFile("/storage/emulated/0/fff.flv");
        }
    }

}
