package com.example.helloneon;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

public class HelloNeon extends AppCompatActivity implements View.OnClickListener{

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_hello_neon);

        Button btn_rgb2gray = findViewById(R.id.button_rgb2gray);
        btn_rgb2gray.setOnClickListener(this);
        Button btn_rgb2yuv = findViewById(R.id.button_rgb2yuv);
        btn_rgb2yuv.setOnClickListener(this);

        ((TextView)findViewById(R.id.text_view_hello_neon))
                .setText(stringFromJNI());
    }

    @Override
    public void onClick(View v) {
        if (v.getId() == R.id.button_rgb2gray) {
            new Thread(new Runnable(){
                @Override
                public void run(){
                    rgb2gray();
                }
            }).start();
        } else if (v.getId() == R.id.button_rgb2yuv) {
            new Thread(new Runnable(){
                @Override
                public void run(){
                    rgb2yuv();
                }
            }).start();
        }
    }

    public native void rgb2gray();
    public native void rgb2yuv();
    public native String stringFromJNI();

    static {
        System.loadLibrary("hello-neon");
    }
}
