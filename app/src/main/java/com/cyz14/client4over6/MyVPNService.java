package com.cyz14.client4over6;

import android.content.Intent;
import android.net.VpnService;
import android.os.IBinder;
import android.util.Log;

/**
 * Created by cyz14 on 2016/5/1.
 */
public class MyVPNService extends VpnService {
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Log.d("MyVPNService", "onCreate executed");
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
}