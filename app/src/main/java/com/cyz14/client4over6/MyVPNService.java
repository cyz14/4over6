package com.cyz14.client4over6;

import android.content.Intent;
import android.net.VpnService;
import android.os.Environment;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Created by cyz14 on 2016/5/1.
 */
public class MyVPNService extends VpnService {
    private static String ipv4Addr = "13.8.0.2";
    private static String router="0.0.0.0";
    private static String dns1="59.66.16.64";
    private static String dns2="8.8.8.8";
    private static String dns3="202.106.0.20";
    static final int MAX_BUF = 1024;
    byte[] readBuf = new byte[MAX_BUF];

    @Override
    public IBinder onBind(Intent intent) {
        ipv4Addr = intent.getStringExtra("ipv4Addr");
        router   = intent.getStringExtra("router");
        dns1     = intent.getStringExtra("dns1");
        dns2     = intent.getStringExtra("dns2");
        dns3     = intent.getStringExtra("dns3");
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Log.d("MyVPNService", "onCreate executed");

        Builder builder = new Builder();
        builder.setMtu(1460);
        Log.d("BadAddress", ipv4Addr);
        builder.addAddress(ipv4Addr, 0); // prefixLength not important?
        builder.addRoute(router, 0); // router is "0.0.0.0" by default
        builder.addDnsServer(dns1);
        builder.addDnsServer(dns2);
        builder.addDnsServer(dns3);
//        builder.addSearchDomain();
        builder.setSession("PacketCapture");
//        builder.setConfigureIntent();

        ParcelFileDescriptor m_interface = builder.establish();
        int fd = m_interface.getFd();

        String tmp = "File descriptor got " + String.valueOf(fd);
        Log.d("MyVPNService", tmp);
        writeFD(fd);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d("MyVPNService", "onStartCommand executed");
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.d("MyVPNService","onDestroy executed");
    }

    private void writeFD(int fd) {
        File cwd = Environment.getExternalStorageDirectory(); // get current work dir
        Log.d("WriteFD", cwd.toString());

        File file = new File(cwd, "cmd_pipe");
        FileOutputStream fileOutputStream;
        try {
            fileOutputStream = new FileOutputStream(file);
        } catch (FileNotFoundException e) {
            Log.e("FILE", "File cmd_pipe not found");
            return;
        }
        BufferedOutputStream out = new BufferedOutputStream(fileOutputStream);
        try {
            out.write(fd);
            out.flush();
            out.close();
        } catch (IOException e) {
            Log.e("IO", "arr byte array not written");
        }

    }

    private void writePipe(byte[] arr) {
        File cwd = Environment.getExternalStorageDirectory(); // get current work dir
        File file = new File(cwd, "cmd_pipe");
        FileOutputStream fileOutputStream;
        try {
            fileOutputStream = new FileOutputStream(file);
        } catch (FileNotFoundException e) {
            Log.e("FILE", "File cmd_pipe not found");
            return;
        }
        BufferedOutputStream out = new BufferedOutputStream(fileOutputStream);
        try {
            out.write(arr, 0, arr.length);
            out.flush();
            out.close();
        } catch (IOException e) {
            Log.e("IO", "arr byte array not written");
        }
    }


}
