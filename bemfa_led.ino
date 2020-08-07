#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#define TCP_SERVER_ADDR "bemfa.com" //巴法云服务器地址默认即可
#define TCP_SERVER_PORT "8344"      //服务器端口，tcp创客云端口8344
#define DEFAULT_STASSID "WIFIID"    //WIFI名称，区分大小写，不要写错
#define DEFAULT_STAPSW "WIFIPSW"    //WIFI密码
#define UID "UID"                   //用户私钥，可在控制台获取,修改为自己的UID
#define TOPIC "ver1"                //主题名字，可在控制台新建
const int LED_Pin = D2;             //单片机LED引脚值

//最大字节数
#define MAX_PACKETSIZE 512
//设置心跳值30s
#define KEEPALIVEATIME 30 * 1000

//tcp客户端相关初始化，默认即可
WiFiClient TCPclient;
String TcpClient_Buff = "";
unsigned int TcpClient_BuffIndex = 0;
unsigned long TcpClient_preTick = 0;
unsigned long preHeartTick = 0;    //心跳
unsigned long preTCPStartTick = 0; //连接
bool preTCPConnected = false;

//相关函数初始化
//连接WIFI
void doWiFiTick();
void startSTA();

//TCP初始化连接
void doTCPClientTick();
void startTCPClient();
void sendtoTCPServer(String p);

//led 控制函数
void turnOnLed();
void turnOffLed();

/*
  *发送数据到TCP服务器
 */
void sendtoTCPServer(String p)
{

    if (!TCPclient.connected())
    {
        Serial.printf("Client is not readly");
        return;
    }
    TCPclient.println(p);
    Serial.printf("[Send to TCPServer]:String");
    Serial.printf(p);
}

/*
  *初始化和服务器建立连接
*/
void startTCPClient()
{
    if (TCPclient.connect(TCP_SERVER_ADDR, atoi(TCP_SERVER_PORT)))
    { //若连接成功
        Serial.printf("\nConnected to server:");
        Serial.printf("%s:%d\r\n", TCP_SERVER_ADDR, atoi(TCP_SERVER_PORT)); //串口输出连接到的服务器信息
        char tcpTemp[128];
        sprintf(tcpTemp, "cmd=1&uid=%s&topic=%s\r\n", UID, TOPIC); //向tcpTemp填充UID和TOPIC

        sendtoTCPServer(tcpTemp);
        preTCPConnected = true;
        preHeartTick = millis();    //心跳长度为运行长度
        TCPclient.setNoDelay(true); //控制是否开启Nagle算法(Nagle算法用于调高广域网传输速率，减小小分组的报文个数)
    }
    else
    {
        Serial.printf("Failed connected to server:"); //连接不成功则串口输出"Failed connected to server"
        Serial.printf(TCP_SERVER_ADDR);               //串口输出服务器IP
        TCPclient.stop();                             //停止服务器连接
        preTCPConnected = false;
    }
    preTCPStartTick = millis();
}

/*
  *检查数据，发送心跳
*/
void doTCPClientTick()
{
    //检查是否断开，断开后重连
    if (WiFi.status() != WL_CONNECTED)
        return;
    if (!TCPclient.connected())
    { //断开重连
        if (preTCPConnected == true)
        {
            preTCPConnected = false;
            preTCPStartTick = millis();
            Serial.printf();
            Serial.printf("TCP Client disconnected.");
            TCPclient.stop();
        }
        else if (millis() - preTCPStartTick > 1 * 1000) //重新连接
            startTCPClient();
    }
    else
    {
        if (TCPclient.available())
        { //收数据
            char c = TCPclient.read();
            TcpClient_Buff += c;
            TcpClient_BuffIndex++;
            TcpClient_preTick = millis();

            if (TcpClient_BuffIndex >= MAX_PACKETSIZE - 1)
            {
                TcpClient_BuffIndex = MAX_PACKETSIZE - 2;
                TcpClient_preTick = TcpClient_preTick - 200;
            }
            preHeartTick = millis();
        }
        if (millis() - preHeartTick >= KEEPALIVEATIME)
        { //保持心跳
            preHeartTick = millis();
            Serial.printf("--Keep alive:");
            sendtoTCPServer("cmd=0&msg=keep\r\n");
        }
    }
    if ((TcpClient_Buff.length() >= 1) && (millis() - TcpClient_preTick >= 200))
    { //data ready
        TCPclient.flush();
        Serial.printf("Buff");
        Serial.println(TcpClient_Buff);
        if ((TcpClient_Buff.indexOf("&msg=on") > 0))
        {
            turnOnLed();
        }
        else if ((TcpClient_Buff.indexOf("&msg=off") > 0))
        {
            turnOffLed();
        }
        TcpClient_Buff = "";
        TcpClient_BuffIndex = 0;
    }
}

void startSTA()
{
    WiFi.disconnect();                           //断开wifi
    WiFi.mode(WIFI_STA);                         //设置为STA模式
    WiFi.begin(DEFAULT_STASSID, DEFAULT_STAPSW); //使用WIFI ID、WIFI PSW连接WiFi
}

/**************************************************************************
                                 WIFI
***************************************************************************/
/*
  WiFiTick
  检查是否需要初始化WiFi
  检查WiFi是否连接上，若连接成功启动TCP Client
  控制指示灯
*/
void doWiFiTick()
{
    static bool startSTAFlag = false;      //初始化STA模式为关
    static bool taskStarted = false;       //初始化任务
    static uint32_t lastWiFiCheckTick = 0; //初始化lastWiFicheckTick时间为0

    if (!startSTAFlag)
    {
        startSTAFlag = true;
        startSTA();
        Serial.printf("Heap size:%d\r\n", ESP.getFreeHeap()); //getFreeHeap():获取资源大小
    }

    //未连接1s重连
    if (WiFi.status() != WL_CONNECTED)
    { //判断WIFI是否连接
        if (millis() - lastWiFiCheckTick > 1000)
        {                                 //若没有，则判断(运行时间-检查WIFI状态时间)是否已经连接超过1s   //millis()返回运行当前程序的时间
            lastWiFiCheckTick = millis(); //若超过1s，运行时间赋值给新的检查WIFI状态时间
        }
    }
    //连接成功建立
    else
    {
        if (taskStarted == false)
        {                                          //判断任务是否未开始
            taskStarted = true;                    //若未开始，设置开始
            Serial.printf("\r\nGet IP Address: "); //串口输出“获取IP”
            Serial.printf(WiFi.localIP());         //串口输出“本地IP”
            startTCPClient();                      //开始与服务器进行连接
        }
    }
}

//打开灯泡
void turnOnLed()
{
    Serial.printf("Turn ON");       //串口输出“Turn ON"
    digitalWrite(LED_BUILTIN, LOW); //低电平，开灯
}
//关闭灯泡
void turnOffLed()
{
    Serial.printf("Turn OFF");       //Serial-串口 串口输出“Turn OFF"
    digitalWrite(LED_BUILTIN, HIGH); //高电平，关灯
}

// 初始化，相当于main 函数
void setup()
{
    Serial.begin(9600);              //初始化串口通信，并将波特率设置为9600
    pinMode(LED_BUILTIN, OUTPUT);    //设置LED引脚为输出模式
    digitalWrite(LED_BUILTIN, HIGH); //设置LED引脚的输出电压为高电平
}

//循环
void loop()
{
    doWiFiTick();      //连接WIFI
    doTCPClientTick(); //TCP初始化连接
}