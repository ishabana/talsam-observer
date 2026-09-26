#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <TouchDrvCSTXXX.hpp>
#include "secrets.h"
#include "collection.h"

static TFT_eSPI screen;
static TouchDrvCSTXXX touch;
static WiFiUDP udp;
static Collection collection;
struct Peer { IPAddress ip; unsigned packets = 0; unsigned marked = 0; uint32_t last = 0; };
static Peer peers[16];
static unsigned peerCount = 0, selected = 0, squareIndex = 0, droppedPeers = 0;
static bool listening = false, touchReady = false, squares = false;
static String notice = "Waiting for sender broadcasts";
static const uint16_t GOLD = 0xE60E;

static int peerIndex(IPAddress ip) {
  for (unsigned i = 0; i < peerCount; ++i) if (peers[i].ip == ip) return i;
  return -1;
}

static void render() {
  screen.fillScreen(TFT_BLACK);
  screen.setTextColor(GOLD, TFT_BLACK);
  screen.drawString("TALSAM / " + String(squares ? "SQUARES" : "DISCOVER"), 6, 2, 2);
  if (squares && collection.count) {
    squareIndex %= collection.count;
    const auto &m = collection.messages[squareIndex];
    screen.drawString("FROM " + IPAddress(m.source).toString() + "  #" + String(m.id), 6, 22, 2);
    screen.drawString("Abjad " + String(m.total) + "   " + String(squareIndex + 1) + "/" + String(collection.count), 6, 42, 2);
    long grid[16];
    if (wafq(m.total, grid)) {
      for (int i = 0; i < 16; ++i) {
        int x = 8 + (i % 4) * 76, y = 63 + (i / 4) * 21;
        screen.drawRect(x, y, 76, 21, GOLD);
        screen.drawCentreString(String(grid[i]), x + 38, y + 2, m.total < 1000000 ? 2 : 1);
      }
    } else screen.drawString("Below 30: no square", 6, 80, 2);
    screen.drawString("BTN0: IPs   BTN14: next square", 6, 150, 2);
    return;
  }
  screen.drawString(listening ? "Listening  " + WiFi.localIP().toString() : "Connecting to WiFi...", 6, 22, 2);
  screen.drawString("IPs " + String(peerCount) + "  Stored " + String(collection.count), 6, 42, 2);
  if (peerCount) {
    const auto &p = peers[selected % peerCount];
    screen.drawString("> " + p.ip.toString(), 6, 64, 4);
    screen.drawString(String(p.packets) + " packets / " + String(p.marked) + " ABJ1", 6, 94, 2);
  }
  screen.drawString(notice.substring(0, 43), 6, 120, 1);
  screen.drawString("BTN14: next IP   BTN0/tap: collect", 6, 150, 2);
}

static void emit(JsonDocument &doc) {
  Serial.print("@@"); serializeJson(doc, Serial); Serial.println();
}
static void error(const String &message) {
  StaticJsonDocument<256> doc;
  doc["type"] = "error"; doc["message"] = message; emit(doc);
}
static void status() {
  StaticJsonDocument<384> doc;
  doc["type"] = "status"; doc["ip"] = WiFi.localIP().toString();
  doc["listening"] = listening; doc["touch"] = touchReady;
  doc["peers"] = peerCount; doc["stored"] = collection.count;
  doc["evicted"] = collection.evicted; doc["ignoredPeers"] = droppedPeers;
  emit(doc);
}
static void listPeers() {
  for (unsigned i = 0; i < peerCount; ++i) {
    StaticJsonDocument<256> doc;
    doc["type"] = "peer"; doc["ip"] = peers[i].ip.toString();
    doc["packets"] = peers[i].packets; doc["abj1"] = peers[i].marked;
    doc["ageSeconds"] = (millis() - peers[i].last) / 1000; emit(doc);
  }
  status();
}

static void collect(IPAddress ip) {
  if (!listening) { error("WiFi is not connected"); return; }
  if (peerIndex(ip) < 0) { error("IP not heard yet. Send a workshop message, then use ips."); return; }
  notice = "Collecting " + ip.toString(); render();
  HTTPClient http;
  http.setConnectTimeout(1500); http.setTimeout(1500);
  http.useHTTP10(true);
  http.begin("http://" + ip.toString() + "/messages.json");
  int code = http.GET();
  if (code != 200 || http.getSize() > 16000) {
    http.end(); notice = "No public records at " + ip.toString();
    error(notice + " (HTTP " + String(code) + ")"); render(); return;
  }
  DynamicJsonDocument doc(18000);
  // The document capacity and read timeout bound responses without buffering an unbounded body.
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::NestingLimit(4));
  http.end();
  int added = err ? -1 : collection.ingest((uint32_t)ip, doc.as<JsonVariantConst>());
  if (added < 0) {
    notice = "Not a valid workshop record list"; error(notice); render(); return;
  }
  notice = String(added) + " new records collected";
  if (collection.count) { squares = true; squareIndex = collection.count - 1; }
  StaticJsonDocument<256> result;
  result["type"] = "collected"; result["ip"] = ip.toString();
  result["added"] = added; result["stored"] = collection.count;
  result["evicted"] = collection.evicted; emit(result); render();
}

static void dump() {
  for (unsigned i = 0; i < collection.count; ++i) {
    const auto &m = collection.messages[i];
    DynamicJsonDocument doc(2048);
    doc["type"] = "message"; doc["id"] = m.id;
    doc["source"] = IPAddress(m.source).toString(); doc["total"] = m.total;
    doc["packet"] = m.packet;
    JsonArray values = doc.createNestedArray("square");
    long grid[16];
    if (wafq(m.total, grid)) for (long n : grid) values.add(n);
    emit(doc);
  }
  status();
}
static void command(String line) {
  line.trim();
  if (line == "ips") listPeers();
  else if (line == "status") status();
  else if (line == "dump") dump();
  else if (line.startsWith("collect ")) {
    IPAddress ip;
    if (ip.fromString(line.substring(8))) collect(ip);
    else error("Use collect followed by an IPv4 address from ips");
  } else error("Commands: ips, collect <ip>, dump, status");
  Serial.println("@@END");
}

void setup() {
  pinMode(15, OUTPUT); digitalWrite(15, HIGH);
  pinMode(14, INPUT_PULLUP); pinMode(0, INPUT_PULLUP);
  Serial.begin(115200);
  screen.init(); screen.setRotation(1);
  touch.setPins(21, 16);
  touchReady = touch.begin(Wire, CST328_SLAVE_ADDRESS, 18, 17);
  if (!touchReady) touchReady = touch.begin(Wire, CST816_SLAVE_ADDRESS, 18, 17);
  if (touchReady) touch.disableAutoSleep();
  WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS); render();
}
void loop() {
  static uint32_t lastStatus = 0, lastPress = 0;
  if (millis() - lastStatus > 1000) {
    lastStatus = millis();
    if (WiFi.status() == WL_CONNECTED && !listening) listening = udp.begin(4646);
    if (WiFi.status() != WL_CONNECTED && listening) { udp.stop(); listening = false; }
    render();
  }
  int size = listening ? udp.parsePacket() : 0;
  if (size > 0) {
    uint8_t header[4];
    int n = udp.read(header, sizeof header);
    IPAddress source = udp.remoteIP();
    while (udp.available()) udp.read();
    int index = peerIndex(source);
    if (index < 0 && peerCount < 16) {
      index = peerCount++; peers[index].ip = source;
      notice = "Heard " + source.toString();
    }
    if (index >= 0) {
      auto &p = peers[index]; ++p.packets; p.last = millis();
      if (size >= 32 && n == 4 && !memcmp(header, "ABJ1", 4)) ++p.marked;
    } else ++droppedPeers;
    render();
  }
  static String line;
  static bool overflow = false;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      if (overflow) { error("Command too long"); Serial.println("@@END"); }
      else command(line);
      line = ""; overflow = false;
    } else if (c != '\r') {
      if (line.length() < 80) line += c; else overflow = true;
    }
  }
  int16_t x[5], y[5];
  bool down = touchReady && touch.getPoint(x, y, 1) > 0;
  bool next = digitalRead(14) == LOW, action = digitalRead(0) == LOW;
  static bool wasDown = false, wasNext = false, wasAction = false;
  if (millis() - lastPress > 300) {
    if ((down && !wasDown) || (action && !wasAction)) {
      lastPress = millis();
      if (squares) { squares = false; render(); }
      else if (peerCount) collect(peers[selected % peerCount].ip);
    } else if (next && !wasNext) {
      lastPress = millis();
      if (squares && collection.count) squareIndex = (squareIndex + 1) % collection.count;
      else if (peerCount) selected = (selected + 1) % peerCount;
      render();
    }
  }
  wasDown = down; wasNext = next; wasAction = action;
  delay(5);
}
