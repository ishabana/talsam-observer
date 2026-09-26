#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "wafq.h"

struct CollectedMessage {
  uint32_t source = 0;
  unsigned id = 0;
  long total = 0;
  String packet;
};

class Collection {
 public:
  static constexpr unsigned LIMIT = 40;
  CollectedMessage messages[LIMIT];
  unsigned count = 0, nextId = 1, evicted = 0;

  // Validate the complete response before changing stored data.
  int ingest(uint32_t source, JsonVariantConst root) {
    if (!root.is<JsonArrayConst>() || root.size() > 10) return -1;
    for (JsonVariantConst item : root.as<JsonArrayConst>()) {
      if (!item["total"].is<long>() || item["total"].as<long>() < 0 ||
          !item["packet"].is<const char *>()) return -1;
      const char *packet = item["packet"];
      size_t n = strlen(packet);
      if (n < 64 || n > 1024 || n % 2 || strncmp(packet, "41424a31", 8)) return -1;
      for (size_t i = 0; i < n; ++i) if (!isxdigit((unsigned char)packet[i])) return -1;
    }
    int added = 0;
    for (JsonVariantConst item : root.as<JsonArrayConst>()) {
      String packet = item["packet"].as<String>();
      bool exists = false;
      for (unsigned i = 0; i < count; ++i)
        if (messages[i].source == source && messages[i].packet == packet) exists = true;
      if (exists) continue;
      if (count == LIMIT) {
        for (unsigned i = 1; i < count; ++i) messages[i - 1] = messages[i];
        --count;
        ++evicted;
      }
      auto &m = messages[count++];
      m.source = source;
      m.id = nextId++;
      m.total = item["total"].as<long>();
      m.packet = packet;
      ++added;
    }
    return added;
  }
};
