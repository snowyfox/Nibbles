// Nibbles usermod for WLED: ties the shark's lights into the Nibbles system.
//
// - Turns ESP-NOW on and acts as the channel anchor: broadcasts anchor
//   heartbeats on whatever channel WLED is on (a phone hotspot at home, its own
//   AP at a festival). The eyes and base station scan until they hear one.
// - Takes Nibbles commands aimed at WLED (preset set / next / previous) and
//   acks them.
// - Broadcasts WLED telemetry, and shows the eyes' telemetry on the Info page.
//
// Messages come from shared/nibbles_link (the same protocol code as the eyes
// and base). See docs/architecture.md in the Nibbles repo.
#include "wled.h"
#include "nibbles_link.h"

#ifdef WLED_DISABLE_ESPNOW
#error "usermod_nibbles needs ESP-NOW"
#endif

#define NIBBLES_WLED_FW        1
#define ANCHOR_HEARTBEAT_MS    250   // 4 Hz: a scanning node dwells 400 ms per channel
#define TELEMETRY_MS           500
#define RX_QUEUE               8

class NibblesUsermod : public Usermod {
  private:
    bool enabled = true;
    bool anchor = true;
    unsigned long lastHeartbeat = 0, lastTelemetry = 0, lastDebug = 0;
    uint16_t seq = 0;

    // Packets arrive on the ESP-NOW callback; handle them in loop().
    struct RxItem { uint8_t mac[6]; uint8_t len; uint8_t data[NL_RADIO_MAX]; };
    RxItem rx[RX_QUEUE];
    volatile uint8_t rxHead = 0, rxTail = 0;
    portMUX_TYPE rxLock = portMUX_INITIALIZER_UNLOCKED;

    // Last command, so a retry (same id) is acked but applied once.
    uint16_t lastCmdId = 0;
    uint8_t lastCmdMac[6] = { 0 };

    // What we know about the eyes.
    nl_eye_telemetry_t eyes = {};
    unsigned long eyesAt = 0;
    bool haveEyes = false;
    uint32_t rxCount = 0, cmdCount = 0;

    static const char _name[];

    bool radioUp() const { return enabled && enableESPNow && statusESPNow == ESP_NOW_STATE_ON; }

    void send(const uint8_t *mac, uint8_t type, const void *payload, size_t len) {
      uint8_t pkt[NL_RADIO_MAX];
      const size_t n = nl_radio_build(pkt, sizeof(pkt), type, seq++, NL_ROLE_WLED, NL_SIDE_NONE, payload, len);
      if (n) quickEspNow.send(mac, pkt, n);
    }

    void sendHeartbeat() {
      nl_heartbeat_t hb = {};
      hb.role = NL_ROLE_WLED;
      hb.side = NL_SIDE_NONE;
      hb.fw = NIBBLES_WLED_FW;
      hb.uptime_ms = millis();
      hb.fps_x10 = strip.getFps() * 10;
      hb.flags = anchor ? NL_HB_ANCHOR : 0;
      hb.channel = WiFi.channel();
      send(ESPNOW_BROADCAST_ADDRESS, NL_MSG_HEARTBEAT, &hb, sizeof(hb));
    }

    void sendTelemetry() {
      nl_wled_telemetry_t t = {};
      t.on = bri > 0;
      t.bri = bri ? bri : briLast;
      t.preset = currentPreset;
      t.fx = strip.getMainSegment().mode;
      t.palette = strip.getMainSegment().palette;
      t.channel = WiFi.channel();
      t.fps = strip.getFps();
      t.leds = strip.getLengthTotal();
      send(ESPNOW_BROADCAST_ADDRESS, NL_MSG_WLED_TELEMETRY, &t, sizeof(t));
    }

    // The next (or previous) preset id that exists, wrapping around.
    int stepPreset(int from, int dir) {
      String name;
      for (int i = 1; i <= 250; i++) {
        int id = from + dir * i;
        while (id < 1) id += 250;
        while (id > 250) id -= 250;
        if (getPresetName(id, name)) return id;
      }
      return 0;
    }

    uint8_t applyCommand(const nl_cmd_t &cmd) {
      if (cmd.target != NL_TARGET_WLED) return NL_ACK_UNSUPPORTED;
      int id;
      switch (cmd.op) {
        case NL_OP_PRESET_SET:  id = cmd.arg; break;
        case NL_OP_PRESET_NEXT: id = stepPreset(currentPreset, +1); break;
        case NL_OP_PRESET_PREV: id = stepPreset(currentPreset, -1); break;
        default: return NL_ACK_UNSUPPORTED;
      }
      String name;
      if (id < 1 || id > 250 || !getPresetName(id, name)) return NL_ACK_BAD_ARG;
      applyPreset(id, CALL_MODE_DIRECT_CHANGE);
      return NL_ACK_OK;
    }

    void handle(const RxItem &it) {
      nl_radio_hdr_t h;
      const uint8_t *pl;
      size_t len;
      if (!nl_radio_parse(it.data, it.len, &h, &pl, &len)) return;
      rxCount++;
      if (h.type == NL_MSG_EYE_TELEMETRY && len == sizeof(nl_eye_telemetry_t)) {
        memcpy(&eyes, pl, sizeof(eyes));
        eyesAt = millis();
        haveEyes = true;
      } else if (h.type == NL_MSG_CMD && len == sizeof(nl_cmd_t)) {
        nl_cmd_t cmd;
        memcpy(&cmd, pl, sizeof(cmd));
        if (cmd.target != NL_TARGET_WLED) return;  // for someone else
        const bool repeat = cmd.id == lastCmdId && memcmp(it.mac, lastCmdMac, 6) == 0;
        static uint8_t lastStatus = NL_ACK_OK;
        if (!repeat) {
          lastStatus = applyCommand(cmd);
          cmdCount++;
#ifdef NIBBLES_DEBUG
          Serial.printf("nibbles: command op %d arg %d -> status %d, preset now %d\n", cmd.op, cmd.arg, lastStatus, currentPreset);
#endif
        }
        lastCmdId = cmd.id;
        memcpy(lastCmdMac, it.mac, 6);
        const nl_ack_t ack = { cmd.id, lastStatus };
        send(it.mac, NL_MSG_ACK, &ack, sizeof(ack));
      }
    }

  public:
    void setup() override {
      // The shark needs ESP-NOW; switch it on before WLED starts networking.
      if (enabled && !enableESPNow) enableESPNow = true;
    }

    void loop() override {
      if (!radioUp()) return;
      for (;;) {
        RxItem it;
        bool got = false;
        portENTER_CRITICAL(&rxLock);
        if (rxTail != rxHead) {
          it = rx[rxTail];
          rxTail = (rxTail + 1) % RX_QUEUE;
          got = true;
        }
        portEXIT_CRITICAL(&rxLock);
        if (!got) break;
        handle(it);
      }
      const unsigned long now = millis();
      if (now - lastHeartbeat >= ANCHOR_HEARTBEAT_MS) {
        lastHeartbeat = now;
        sendHeartbeat();
      }
      if (now - lastTelemetry >= TELEMETRY_MS) {
        lastTelemetry = now;
        sendTelemetry();
      }
#ifdef NIBBLES_DEBUG
      if (now - lastDebug >= 5000) {
        lastDebug = now;
        Serial.printf("nibbles: ch %d%s, rx %lu, cmds %lu, preset %d, fps %u | eyes %s preset %d/%d %s %.1f/%.1f fps\n",
                      WiFi.channel(), anchor ? " anchor" : "", (unsigned long)rxCount, (unsigned long)cmdCount,
                      currentPreset, strip.getFps(), haveEyes && now - eyesAt < 2000 ? "heard" : "NOT heard",
                      eyes.preset, eyes.preset_count, eyes.linked ? "linked" : "unlinked",
                      eyes.fps_x10[0] / 10.0f, eyes.fps_x10[1] / 10.0f);
      }
#endif
    }

    bool onEspNowMessage(uint8_t *sender, uint8_t *payload, uint8_t len) override {
      if (!enabled || len < sizeof(nl_radio_hdr_t) || payload[0] != NL_RADIO_MAGIC0 || payload[1] != NL_RADIO_MAGIC1)
        return false;  // not ours: let WLED handle it (e.g. WiZ remotes)
      portENTER_CRITICAL(&rxLock);
      const uint8_t next = (rxHead + 1) % RX_QUEUE;
      if (next != rxTail && len <= NL_RADIO_MAX) {
        memcpy(rx[rxHead].mac, sender, 6);
        rx[rxHead].len = len;
        memcpy(rx[rxHead].data, payload, len);
        rxHead = next;
      }
      portEXIT_CRITICAL(&rxLock);
      return true;
    }

    void addToJsonInfo(JsonObject &root) override {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");
      JsonArray radio = user.createNestedArray(F("Nibbles radio"));
      if (!radioUp()) {
        radio.add(F("off"));
      } else {
        char buf[48];
        snprintf(buf, sizeof(buf), "ch %d%s, rx %lu, cmds %lu", WiFi.channel(), anchor ? " (anchor)" : "",
                 (unsigned long)rxCount, (unsigned long)cmdCount);
        radio.add(buf);
      }
      JsonArray e = user.createNestedArray(F("Nibbles eyes"));
      if (!haveEyes || millis() - eyesAt > 2000) {
        e.add(F("not heard"));
      } else {
        char buf[80];
        snprintf(buf, sizeof(buf), "preset %d/%d, %s, %.1f/%.1f fps, %.0f bpm", eyes.preset, eyes.preset_count,
                 eyes.linked ? "linked" : "not linked", eyes.fps_x10[0] / 10.0f, eyes.fps_x10[1] / 10.0f, eyes.tempo_bpm);
        e.add(buf);
      }
    }

    void addToConfig(JsonObject &root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[F("enabled")] = enabled;
      top[F("anchor")] = anchor;
    }

    bool readFromConfig(JsonObject &root) override {
      JsonObject top = root[FPSTR(_name)];
      bool complete = !top.isNull();
      complete &= getJsonValue(top[F("enabled")], enabled, true);
      complete &= getJsonValue(top[F("anchor")], anchor, true);
      return complete;
    }

    uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};

const char NibblesUsermod::_name[] PROGMEM = "Nibbles";

static NibblesUsermod nibbles_usermod;
REGISTER_USERMOD(nibbles_usermod);
