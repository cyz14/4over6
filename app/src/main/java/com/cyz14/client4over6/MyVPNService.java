package com.cyz14.client4over6;

import android.content.Intent;
import android.net.VpnService;
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
public class MyVPNService extends VpnService implements Runnable{

    private Thread mThread;
    static final int MAX_BUF = 1024;
    byte[] readBuf = new byte[MAX_BUF];
    String ipv4Addr;// = "13.8.0.2";
    String router;//="0.0.0.0";
    String dns1;//="59.66.16.64";
    String dns2;//="8.8.8.8";
    String dns3;//="202.106.0.20";
    String sockfd;
    Builder builder = new Builder();

    @Override
    public void onCreate() {
        super.onCreate();
    }

    @Override
    public synchronized void run() {
        Log.d("CYZ_MyVPNService", "onCreate executed");
        builder.setMtu(1460);
        try {
            builder.addAddress(ipv4Addr, 32); // prefixLength not important?
        } catch (IllegalArgumentException e) {
            e.printStackTrace();
            Log.e("CYZ_ERROR", ipv4Addr);
        }

        builder.addRoute("0.0.0.0", 0); // router is "0.0.0.0" by default

        builder.addDnsServer(dns1);
        builder.addDnsServer(dns2);
        builder.addDnsServer(dns3);
//              builder.addSearchDomain();
        builder.setSession("PacketCapture");
//              builder.setConfigureIntent();
        if (protect(Integer.parseInt(sockfd))) {
            Log.d("CYZ_PROTECT", "SockFD protected "+ sockfd);
        } else {
            Log.e("CYZ_BUG", "SockFd not protected " + sockfd);
        }
        ParcelFileDescriptor m_interface = builder.establish();
        if (m_interface == null) Log.e("CYZ_Error", "M_interface is null");
        int fd = m_interface.getFd();
        Log.d("CYZ_MyVPNService", "File descriptor got " + String.valueOf(fd));
        //m_interface.detachFd();
        writeFD(fd);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {

        if (mThread != null) {
            mThread.interrupt();
        }
        try {
            ipv4Addr = intent.getStringExtra("ipv4Addr");
            if (ipv4Addr == null) Log.e("CYZ_BUG", "Ipv4 is null");
            else Log.d("CYZ_VPN", ipv4Addr);

            router   = intent.getStringExtra("router");
            if (router == null) Log.e("CYZ_BUG", "Router is null");
            else Log.d("CYZ_VPN", router);

            dns1     = intent.getStringExtra("dns1");
            Log.d("CYZ_VPN", dns1);
            dns2     = intent.getStringExtra("dns2");
            if (dns2 == null) Log.e("CYZ_BUG", "DNS2 is NULL");
            else Log.d("CYZ_INFO", "DNS2 is " + dns2);
            Log.d("CYZ_VPN", dns2);

            dns3     = intent.getStringExtra("dns3");
            if (dns3 == null) {
                Log.d("CYZ_BUG", "DNS3 is NULL");
            } else
            {
                Log.d("CYZ_INFO", "DNS3 is " + dns3);
            }
            Log.d("CYZ_VPN", dns3);

            sockfd = intent.getStringExtra("sockfd");

            Log.d("CYZ_SOCKFD", sockfd);

        } catch (NullPointerException e) {
            e.printStackTrace();
            Log.e("CYZ_BUG", "Null pointer exception in Builder");
        }

        mThread = new Thread(this, "MyVPNService");
        mThread.start();
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mThread != null) {
            mThread.interrupt();
        }
        Log.d("CYZ_MyVPNService","onDestroy executed");
    }

    private void writeFD(int fd) {
        String cwd = getApplicationInfo().dataDir;
        Log.d("CYZ_WriteFD", cwd+fd);

        File file = new File("/data/data/com.cyz14.client4over6/tun_pipe");
        FileOutputStream fileOutputStream;
        try {
            fileOutputStream = new FileOutputStream(file);
        } catch (FileNotFoundException e) {
            Log.d("CYZ_FILE", "File tun_pipe not found");
            return;
        }
        BufferedOutputStream out = new BufferedOutputStream(fileOutputStream);
        try {
            String towrite = String.valueOf(fd);
            out.write(towrite.getBytes());
            out.flush();
            out.close();
        } catch (IOException e) {
            Log.d("CYZ_IO", "arr byte array not written");
        }
    }
}
