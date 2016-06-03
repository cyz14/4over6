package com.cyz14.client4over6;import android.app.Activity;import android.content.Context;import android.content.Intent;import android.net.ConnectivityManager;import android.net.NetworkInfo;import android.net.VpnService;import android.net.wifi.WifiInfo;import android.net.wifi.WifiManager;import android.os.Bundle;import android.os.Environment;import android.os.Handler;import android.os.Message;import android.util.Log;import android.view.Menu;import android.view.MenuItem;import android.view.View;import android.widget.Button;import android.widget.EditText;import android.widget.TextView;import android.widget.Toast;import java.io.BufferedInputStream;import java.io.BufferedOutputStream;import java.io.File;import java.io.FileInputStream;import java.io.FileNotFoundException;import java.io.FileOutputStream;import java.io.IOException;import java.net.InetAddress;import java.net.NetworkInterface;import java.net.SocketException;import java.util.Enumeration;import java.util.Timer;import java.util.TimerTask;public class MainActivity extends Activity {    private static int MAX_BUF = 4096;    int ipPipeFlag = 0;    public static String macAddr;    public static String ipv6Addr;    private static final int MSG_UPDATEUI = 0;    private static final int MSG_CONNECT  = 1;    private static final int MSG_STATUS   = 2;    final String path = "/data/data/com.cyz14.client4over6/cmd_pipe";    private static String ipv4Addr;    private static String router;    private static String dns1;    private static String dns2;    private static String dns3;    private static String sockfd;    private static String[] info;    byte[] readBuf = new byte[MAX_BUF];    private Timer mTimer;    private TimerTask mTimerTask;    private Thread mJNIThread;    private Thread mReadPipeThread;    private Handler mHandler = new Handler() {        public void handleMessage (Message msg) { // in main(UI) thread            EditText editText = (EditText) findViewById(R.id.editText);            switch (msg.what) {                case MSG_UPDATEUI:                    Log.d("CYZ_Handler", "MSG_UPDATEUI");                    break;                case MSG_STATUS:                    String status = (String)msg.obj;                    Log.d("CYZ_STATUS", status);                    editText.append(status + "\n");                    break;                case MSG_CONNECT:                    String temp = (String)msg.obj;                    Log.d("CYZ_Connect", "MSG_CONNECT " + temp);                    info = temp.split(" ");                    Log.d("CYZ_INFO", "info.split length " + info.length);                    if (info.length == 6) {                        ipv4Addr = info[0];                        Log.d("CYZ_INFO", "ipv4Addr.split length " + ipv4Addr.split("\\.").length);                        if (ipv4Addr.split("\\.").length == 4) {                            router   = info[1];                            dns1     = info[2];                            dns2     = info[3];                            dns3     = info[4];                            sockfd   = info[5];                            editText.append(ipv4Addr + "\n");                            editText.append(router + "\n");                            editText.append(dns1 + "\n");                            editText.append(dns2 + "\n");                            editText.append(dns3 + "\n");                            editText.append(info[5] + "\n");                            Log.d("CYZ_VPN", "Will start VPN now");                            startVPNService();                        } else {                            Log.d("CYZ_VPN", temp);                        }                    } else {                        Log.e("CYZ_Bug", "Wrong info from server.");                    }                    break;                default:                    break;            }        }    };    // Java_com_cyz14_client4over6_MainActivity_RequestInfo    public native String RequestInfo(); // IP address, route, DNS    static {        System.loadLibrary("connecttoserver");    }    @Override    protected void onCreate(Bundle savedInstanceState) {        super.onCreate(savedInstanceState);        setContentView(R.layout.activity_main);        ipPipeFlag = 0;        Button testButton = (Button) findViewById(R.id.TestButton);        testButton.setOnClickListener(new View.OnClickListener() {            @Override            public void onClick(View v) {            }        });        final Button connectButton = (Button) findViewById(R.id.ConnectButton);        connectButton.setOnClickListener(new View.OnClickListener() {            @Override            public void onClick(View view) {                Toast.makeText(MainActivity.this, "Trying to start VPN Service", Toast.LENGTH_SHORT).show();                Runnable mJNIRunnable = new Runnable()                {                    @Override                    public void run()                    {                        Log.d("CYZ_JNI", "Start JNI thread");                        String temp = RequestInfo();    // try to connect to server and get ip address                        Log.d("CYZ_JNI", "FD Info:" + temp);                        Message message = new Message();                        message.what = MSG_STATUS;                        message.obj  = temp;                        mHandler.sendMessage(message);                    }                };                Log.d("CYZ_JNI", "Out Thread: Try to start C Thread");                mJNIThread = new Thread(mJNIRunnable);                mJNIThread.start();                mReadPipeThread = new Thread() {                    @Override                    public void run() {                        String temp = new String();                        Log.d("CYZ_PIPE", "IpPipeFlag = 0");                        while (ipPipeFlag == 0) {                            temp = getInfosFromFIFO();                        }                        Log.d("CYZ_ReadFIFO", "Info from FIFO: " + temp);                        Message toMain = new Message();                        toMain.obj = temp;                        if (temp.split(" ").length == 6 && temp.contains("0.0.0.0"))                        {                            toMain.what = MSG_CONNECT;                        } else {                            toMain.what = MSG_STATUS;                        }                        mHandler.sendMessage(toMain);                        Log.d("CYZ_Loop", "Start read FIFO loop");                        while (true) {                            temp = readFIFO();                            Message status = new Message();                            status.what = MSG_STATUS;                            status.obj = temp;                            mHandler.sendMessage(status); // toMain should not be sent again                        }                    }                };                mReadPipeThread.start();            }        });        Button disconnectButton = (Button) findViewById(R.id.DisconnectButton);        disconnectButton.setOnClickListener(new View.OnClickListener() {            @Override            public void onClick(View view) {                Intent intent = new Intent(MainActivity.this, MyVPNService.class);                stopService(intent);                ipPipeFlag = 0;            }        });        Button checkButton = (Button) findViewById(R.id.CheckButton);        checkButton.setOnClickListener(new View.OnClickListener() {            @Override            public void onClick(View v) {                Toast.makeText(MainActivity.this, "Checking net status...", Toast.LENGTH_SHORT).show();                if (ipPipeFlag == 0) checkNetStatus();            }        });        Button finishButton  = (Button) findViewById(R.id.FinishButton);        finishButton.setOnClickListener(new View.OnClickListener() {            @Override            public void onClick(View v) {                finish();            }        });        EditText editText = (EditText)findViewById(R.id.editText);        editText.clearFocus();        editText.setText("Hello 4over6\n");    }    private boolean isNetConnected() {        ConnectivityManager connectivityManager =                (ConnectivityManager) getSystemService(Context.CONNECTIVITY_SERVICE);        if (connectivityManager != null) {            NetworkInfo networkInfo = connectivityManager.getActiveNetworkInfo();            if (networkInfo != null) {                if (networkInfo.isConnected()) {                    return true;                }            }        }        return false;    }    // check if wifi connected    private boolean isWifiConnected() {        ConnectivityManager connectivityManager = (ConnectivityManager)                getSystemService(Context.CONNECTIVITY_SERVICE);        if (connectivityManager != null) {            NetworkInfo networkInfo = connectivityManager.getActiveNetworkInfo();            if (networkInfo != null && networkInfo.getType() == ConnectivityManager.TYPE_WIFI) {                return true;            }        }        return false;    }    private void checkNetStatus() {        TextView statusTextView = (TextView) findViewById(R.id.NetworkStatusTextView);        if (!isNetConnected()) {            Toast.makeText(MainActivity.this, "You are not connected to network", Toast.LENGTH_SHORT).show();            statusTextView.setText("You are not connected to network" + "\n");            return;        }        if (!isWifiConnected()) {            Toast.makeText(MainActivity.this, "You are not connected by WiFi", Toast.LENGTH_SHORT).show();            statusTextView.setText("You are not connected by WiFi\n");            return;        }        statusTextView.setText("You are connected by WiFi\n");        macAddr = getMacAddress();        TextView macTextView = (TextView) findViewById(R.id.MacTextView);        macTextView.setText(macAddr);        ipv6Addr = getIPv6Address();        TextView textView = (TextView)findViewById(R.id.IPv6AddressTextView);        textView.setText(ipv6Addr);        Log.d("CYZ_IPAddress", ipv6Addr);    }    private String getMacAddress() {        WifiManager wifiManager = (WifiManager) getSystemService(WIFI_SERVICE);        WifiInfo wifiInfo = wifiManager.getConnectionInfo();        return wifiInfo.getMacAddress();    }    private String getIPv6Address() {        try {            final Enumeration<NetworkInterface> e = NetworkInterface.getNetworkInterfaces();            while (e.hasMoreElements()) {                final NetworkInterface networkInterface = e.nextElement();                for (Enumeration<InetAddress> enumAddress = networkInterface.getInetAddresses();                     enumAddress.hasMoreElements(); ) {                    InetAddress inetAddress = enumAddress.nextElement();                    if (!inetAddress.isLoopbackAddress() && !inetAddress.isLinkLocalAddress()) {                        return inetAddress.getHostAddress();                    }                }            }        } catch (SocketException e) {            Log.wtf("WIFI_IP", "Unable to NetworkInterface.getNetworkInterfaces()");        }        return null;    }    private boolean startVPNService() {        Intent intent = VpnService.prepare(this);        if (intent != null) {            intent.putExtra("ipv4Addr", ipv4Addr);            intent.putExtra("router", router);            intent.putExtra("dns1", dns1);            intent.putExtra("dns2", dns2);            intent.putExtra("dns3", dns3);            intent.putExtra("sockfd", sockfd);            startActivityForResult(intent, 0);        } else {            onActivityResult(0, RESULT_OK, null);        }        return true;    }    protected void onActivityResult(int request, int result, Intent data) {        if (result == RESULT_OK) {            Intent intent = new Intent(MainActivity.this, MyVPNService.class);            intent.putExtra("ipv4Addr", ipv4Addr);            intent.putExtra("router", router);            intent.putExtra("dns1", dns1);            intent.putExtra("dns2", dns2);            intent.putExtra("dns3", dns3);            intent.putExtra("sockfd", sockfd);            startService(intent);        }    }    // return the length of read    private String readFIFO() {        FileInputStream fileInputStream;        int readLength = 0;        String cwd = getApplicationInfo().dataDir;        File file = new File(cwd, "cmd_pipe");        if (!file.exists()) {            Log.d("CYZ_BUG", "cmd_pipe not exists.");        }        String ret = new String();        try {            fileInputStream = new FileInputStream(file);            BufferedInputStream in = new BufferedInputStream(fileInputStream);            readLength = in.read(readBuf);            in.close();            if (readLength == -1) {                Log.e("CYZ_STR", "Read length -1");                return ret;            }            ret = new String(readBuf);            ret = ret.substring(0, readLength);        } catch (FileNotFoundException e) {            Log.e("CYZ_BUG", "Read pipe file not found");        } catch (IOException e) {            Log.e("CYZ_Exception", "IOException");        } catch (StringIndexOutOfBoundsException e) {            e.printStackTrace();            Log.e("CYZ_BUG", "Read buf too long " + String.valueOf(readLength));        }        return ret.substring(0, readLength);    }    private int writeFIFO(byte[] data) {        File extDir = Environment.getExternalStorageDirectory();        File file = new File(extDir, "cmd_pipe");        try {            FileOutputStream fileOutputStream = new FileOutputStream(file);            BufferedOutputStream bufferedOutputStream =                    new BufferedOutputStream(fileOutputStream);            bufferedOutputStream.write(data, 0, data.length);            bufferedOutputStream.flush();            bufferedOutputStream.close();        } catch (Exception e) {            e.printStackTrace();            return -1;        }        return 0;    }    private String getInfosFromFIFO() {        byte[] buffer = new byte[1024];        String ret;        String cwd = getApplicationInfo().dataDir;        File file = new File(cwd, "cmd_pipe");        try {            FileInputStream fileInputStream = new FileInputStream(file);            BufferedInputStream in = new BufferedInputStream(fileInputStream);            Log.d("CYZ_Infos", "Buffered input stream opened");            int len = 0;            if (ipPipeFlag == 0) {                len = in.read(buffer);                in.close();                Log.d("CYZ_ReadLen", "pipe read length: " + String.valueOf(len));                if (len > 0) {                    ret = new String(buffer);                    Log.d("CYZ_PIPE", ret.substring(0, len));                    ret = ret.substring(0, len);                    String[] temps = ret.split(" ");                    if (temps.length == 6 && temps[0].split("\\.").length == 4)                        ipPipeFlag = 1;                    return ret;                }            }        } catch (Exception e) {            Log.d("CYZ_BUG", "File not found");            e.printStackTrace();        }        return new String("Already found");    }    public boolean onCreateOptionsMenu(Menu menu) {        // Inflate the menu; this adds items to the action bar if it is present.        getMenuInflater().inflate(R.menu.menu_main, menu);        return true;    }    public boolean onOptionsItemSelected(MenuItem item) {        // Handle action bar item clicks here. The action bar will        // automatically handle clicks on the Home/Up button, so long        // as you specify a parent activity in AndroidManifest.xml.        int id = item.getItemId();        //noinspection SimplifiableIfStatement        if (id == R.id.action_settings) {            return true;        }        return super.onOptionsItemSelected(item);    }}