#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include "DHT.h"

#define DHTPIN 25
#define DHTTYPE DHT11
#define RELAY_PIN 26

const char* ssid = "llxspace";  // 替换为您的WiFi名称
const char* password = "llxspace";  // 替换为您的WiFi密码

const char* mqtt_server = "your_mqtt_broker"; // 替换为您的MQTT代理地址
const char* mqtt_user = "your_mqtt_user"; // 替换为您的MQTT用户名
const char* mqtt_password = "your_mqtt_password"; // 替换为您的MQTT密码

WiFiClient espClient;
PubSubClient client(espClient);

DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

bool ventilationOn = false; // 记录通风状态的变量
bool lastVentilationOn = false; // 记录上次的通风状态
unsigned long lastMsg = 0; // 上次发布消息的时间
const long interval = 60000; // 定时发布的间隔时间，单位为毫秒

void setup() {
  Serial.begin(9600);
  pinMode(RELAY_PIN, OUTPUT); // 设置为默认输出
  digitalWrite(RELAY_PIN, LOW); // 默认关闭通风

  dht.begin();
  
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  server.on("/", handleRoot);
  server.on("/toggle-ventilation", handleToggleVentilation);
  server.on("/seat-status", handleSeatStatus);
  server.begin();
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.subscribe("seat/control");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  server.handleClient();

  unsigned long now = millis();
  if (now - lastMsg > interval) {
    lastMsg = now;
    client.publish("seat/status", createStatusPayload().c_str());
  }

  // 检查通风状态是否改变
  if (ventilationOn != lastVentilationOn) {
    lastVentilationOn = ventilationOn;
    client.publish("seat/status", createStatusPayload().c_str());
  }
}

void handleRoot() {
  String message = "<html><head><meta charset='UTF-8'></head><body><h1>ESP32 Control Panel</h1>"
                   "<p><a href=\"/toggle-ventilation\">开关座椅通风</a></p>"
                   "<p><a href=\"/seat-status\">获取座椅的通风及温湿度状态</a></p>"
                   "</body></html>";
  server.send(200, "text/html", message);
}

void handleToggleVentilation() {
  ventilationOn = !ventilationOn; // 切换通风状态

  if (ventilationOn) {
    digitalWrite(RELAY_PIN, HIGH); // 开启通风
  } else {
    digitalWrite(RELAY_PIN, LOW); // 关闭通风
  }

  String state = ventilationOn ? "ON" : "OFF";
  client.publish("seat/status", createStatusPayload().c_str());

  server.send(200, "text/html", "<html><head><meta charset='UTF-8'></head><body><h1>座椅通风已" + state + "</h1>"
                                "<p><a href=\"/\">返回主菜单</a></p></body></html>");
}

void handleSeatStatus() {
  String state = ventilationOn ? "开启" : "关闭";
  String message = "<html><head><meta charset='UTF-8'></head><body><h1>座椅的通风及温湿度状态</h1>"
                   "<p>通风状态: " + state + "</p>"
                   "<p>湿度: " + String(dht.readHumidity(), 1) + "%</p>"
                   "<p>温度: " + String(dht.readTemperature(), 1) + "°C</p>"
                   "<p><a href=\"/\">返回主菜单</a></p></body></html>";
  server.send(200, "text/html", message);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == "seat/control") {
    if (message == "set_ventilation:on") {
      ventilationOn = true;
      digitalWrite(RELAY_PIN, HIGH);
    } else if (message == "set_ventilation:off") {
      ventilationOn = false;
      digitalWrite(RELAY_PIN, LOW);
    }
    // 检查通风状态是否改变
    if (ventilationOn != lastVentilationOn) {
      lastVentilationOn = ventilationOn;
      client.publish("seat/status", createStatusPayload().c_str());
    }
  }
}

String createStatusPayload() {
  String state = ventilationOn ? "on" : "off";
  String payload = "{\"temperature\":\"" + String(dht.readTemperature(), 1) + "°C\","
                   "\"humidity\":\"" + String(dht.readHumidity(), 1) + "%\","
                   "\"ventilation\":\"" + state + "\"}";
  return payload;
}
