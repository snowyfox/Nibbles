// Nibbles usermod for WLED: ties the shark's lights into the Nibbles system.
//
// - Turns ESP-NOW on and acts as the channel anchor: broadcasts anchor
//   heartbeats on whatever channel WLED is on (a phone hotspot at home, its own
//   AP at a festival). The eyes and base station scan until they hear one.
// - Takes Nibbles commands aimed at WLED (preset set / next / previous) and
//   acks them.
// - Broadcasts WLED telemetry, and shows the eyes' telemetry on the Info page.
// - Bumps: momentary effects (flash, blackout, preset) while a base-station
//   button is held; WLED's previous state is restored exactly afterwards.
//
// Messages come from shared/nibbles_link (the same protocol code as the eyes
// and base). See docs/architecture.md in the Nibbles repo.
#include "wled.h"
#include "nibbles_link.h"
#include <map>

#ifdef WLED_DISABLE_ESPNOW
#error "usermod_nibbles needs ESP-NOW"
#endif

#define NIBBLES_WLED_FW        1
#define ANCHOR_HEARTBEAT_MS    250   // 4 Hz: a scanning node dwells 400 ms per channel
#define TELEMETRY_MS           500
#define AUDIO_MS               32     // audio features, like the port eye sends them
#define RX_QUEUE               8
#define JSON_LOCK_NIBBLES      240

class NibblesUsermod : public Usermod {
  private:
    bool enabled = true;
    bool anchor = true;
    bool audio = false;  // publish AudioReactive's analysis for the eyes
    unsigned long lastHeartbeat = 0, lastTelemetry = 0, lastDebug = 0, lastAudio = 0;
    nl_ar_state_t arState;
    uint32_t audioSent = 0;
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
    uint32_t rxCount = 0, cmdCount = 0, bumpCount = 0;

    // Which preset ids exist, read from presets.json once and again only when
    // presets change (reading the file per id is far too slow for a command).
    uint8_t presetBits[32] = { 0 };
    std::map<uint8_t, String> presetNames;  // for telemetry, filled with presetBits
    bool presetBitsValid = false;
    unsigned long presetBitsTime = 0;

    // Bumps: WLED's state is saved as JSON when one begins and put back when it ends.
    nl_bump_rx_t bumps;
    String savedState;
    bool haveSaved = false;
    uint8_t savedPreset = 0;

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

    // AudioReactive's analysis as Nibbles audio features (see nl_ar_update).
    void sendAudio() {
      um_data_t *um = nullptr;
      if (!UsermodManager::getUMData(&um, USERMOD_ID_AUDIOREACTIVE) || !um) return;
      const float volume = *(float *)um->u_data[0];
      const uint8_t *fft = (const uint8_t *)um->u_data[2];
      const bool peak = *(uint8_t *)um->u_data[3];
      nl_audio_t a;
      nl_ar_update(&arState, volume, fft, peak, millis(), &a);
      send(ESPNOW_BROADCAST_ADDRESS, NL_MSG_AUDIO, &a, sizeof(a));
      audioSent++;
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
      refreshPresets();  // only reads the file after presets change
      auto it = presetNames.find(currentPreset);
      if (currentPreset && it != presetNames.end()) strlcpy(t.name, it->second.c_str(), sizeof(t.name));
      send(ESPNOW_BROADCAST_ADDRESS, NL_MSG_WLED_TELEMETRY, &t, sizeof(t));
    }

    void refreshPresets() {
      if (presetBitsValid && presetBitsTime == presetsModifiedTime) return;
      JSONBufferGuard guard(JSON_LOCK_NIBBLES);
      if (!guard) return;
      memset(presetBits, 0, sizeof(presetBits));
      presetNames.clear();
      File f = WLED_FS.open(F("/presets.json"), "r");
      if (!f) {
        presetBitsValid = true;  // no presets file: no presets
        presetBitsTime = presetsModifiedTime;
        return;
      }
      pDoc->clear();
      const DeserializationError err = deserializeJson(*pDoc, f);
      f.close();
      if (err) return;  // leave invalid; try again next time
      for (JsonPair kv : pDoc->as<JsonObject>()) {
        const int id = atoi(kv.key().c_str());
        if (id < 1 || id > 250) continue;
        presetBits[id / 8] |= 1 << (id % 8);
        const char *name = kv.value()["n"] | "";
        if (*name) presetNames[id] = name;
      }
      presetBitsValid = true;
      presetBitsTime = presetsModifiedTime;
    }

    bool presetExists(int id) const {
      return id >= 1 && id <= 250 && (presetBits[id / 8] & (1 << (id % 8)));
    }

    // The next (or previous) preset id that exists, wrapping around; 0 if none.
    int stepPreset(int from, int dir) const {
      for (int i = 1; i <= 250; i++) {
        int id = from + dir * i;
        while (id < 1) id += 250;
        while (id > 250) id -= 250;
        if (presetExists(id)) return id;
      }
      return 0;
    }

    // Work out what a command means without doing it (fast, so the ack goes out at once).
    // Works out what a command will do: a preset id, or a brightness (id < 0
    // means -brightness). Returns the ack status.
    uint8_t resolveCommand(const nl_cmd_t &cmd, int &id) {
      if (cmd.target != NL_TARGET_WLED) return NL_ACK_UNSUPPORTED;
      switch (cmd.op) {
        case NL_OP_BRIGHTNESS_SET:
          if (cmd.arg < 0 || cmd.arg > 255) return NL_ACK_BAD_ARG;
          id = -cmd.arg;
          return NL_ACK_OK;
        case NL_OP_BRIGHTNESS_STEP:
          id = -constrain((int)bri + cmd.arg, 1, 255);
          return NL_ACK_OK;
        default: break;
      }
      refreshPresets();
      switch (cmd.op) {
        case NL_OP_PRESET_SET:  id = cmd.arg; break;
        case NL_OP_PRESET_NEXT: id = stepPreset(currentPreset, +1); break;
        case NL_OP_PRESET_PREV: id = stepPreset(currentPreset, -1); break;
        default: return NL_ACK_UNSUPPORTED;
      }
      return presetExists(id) ? NL_ACK_OK : NL_ACK_BAD_ARG;
    }

    void applyCommand(int id) {
      if (id > 0) {
        applyPreset(id, CALL_MODE_DIRECT_CHANGE);
      } else {
        bri = -id;
        stateUpdated(CALL_MODE_DIRECT_CHANGE);
      }
    }

    bool saveState() {
      JSONBufferGuard guard(JSON_LOCK_NIBBLES);
      if (!guard) return false;
      pDoc->clear();
      JsonObject st = pDoc->to<JsonObject>();
      serializeState(st, true);
      savedState = "";
      serializeJson(*pDoc, savedState);
      return true;
    }

    bool applyJson(const String &json) {
      JSONBufferGuard guard(JSON_LOCK_NIBBLES);
      if (!guard) return false;
      pDoc->clear();
      if (deserializeJson(*pDoc, json)) return false;
      deserializeState(pDoc->as<JsonObject>(), CALL_MODE_DIRECT_CHANGE);
      return true;
    }

    void bumpBegin(const nl_bump_t &b) {
      haveSaved = saveState();
      savedPreset = currentPreset;
      bumpCount++;
      if (b.action == NL_BUMP_PRESET) {
        refreshPresets();
        if (presetExists(b.arg)) applyPreset(b.arg, CALL_MODE_DIRECT_CHANGE);
        return;
      }
      // Instant change (tt = transition 0 for this call only), every segment.
      String json = "{\"tt\":0,";
      if (b.action == NL_BUMP_BLACKOUT) {
        json += "\"on\":false}";
      } else {  // NL_BUMP_FLASH
        json += "\"on\":true,\"bri\":" + String(b.intensity ? b.intensity : 255) + ",\"seg\":[";
        for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
          if (i) json += ",";
          json += "{\"id\":" + String(i) + ",\"fx\":0,\"col\":[[255,255,255]]}";
        }
        json += "]}";
      }
      applyJson(json);
    }

    void bumpEnd() {
      if (!haveSaved) return;
      // Put everything back instantly.
      String json = savedState;
      if (json.startsWith("{")) json = "{\"tt\":0," + json.substring(1);
      applyJson(json);
      currentPreset = savedPreset;  // restoring the state clears it; it's the same look
      haveSaved = false;
    }

    void bumpEvent(nl_bump_event_t ev) {
      if (ev == NL_BUMP_EV_BEGIN) bumpBegin(bumps.bump);
      else if (ev == NL_BUMP_EV_END) bumpEnd();
      else if (ev == NL_BUMP_EV_REPLACE) {
        bumpEnd();
        bumpBegin(bumps.bump);
      }
#ifdef NIBBLES_DEBUG
      if (ev != NL_BUMP_EV_NONE)
        Serial.printf("nibbles: bump %s (action %d)\n", ev == NL_BUMP_EV_END ? "end" : "begin", bumps.bump.action);
#endif
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
      } else if (h.type == NL_MSG_BUMP && len == sizeof(nl_bump_t)) {
        nl_bump_t b;
        memcpy(&b, pl, sizeof(b));
        if (b.target & NL_TARGET_WLED) bumpEvent(nl_bump_rx_message(&bumps, &b, millis()));
      } else if (h.type == NL_MSG_CMD && len == sizeof(nl_cmd_t)) {
        nl_cmd_t cmd;
        memcpy(&cmd, pl, sizeof(cmd));
        if (cmd.target != NL_TARGET_WLED) return;  // for someone else
        const bool repeat = cmd.id == lastCmdId && memcmp(it.mac, lastCmdMac, 6) == 0;
        static uint8_t lastStatus = NL_ACK_OK;
        int id = 0;
        if (!repeat) lastStatus = resolveCommand(cmd, id);
        lastCmdId = cmd.id;
        memcpy(lastCmdMac, it.mac, 6);
        const nl_ack_t ack = { cmd.id, lastStatus };
        send(it.mac, NL_MSG_ACK, &ack, sizeof(ack));  // ack first, then act
        if (!repeat) {
          cmdCount++;
          if (lastStatus == NL_ACK_OK) applyCommand(id);
#ifdef NIBBLES_DEBUG
          Serial.printf("nibbles: command op %d arg %d -> status %d, preset %d\n", cmd.op, cmd.arg, lastStatus, id);
#endif
        }
      }
    }

  public:
    void setup() override {
      nl_bump_rx_init(&bumps);
      nl_ar_init(&arState);
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
      bumpEvent(nl_bump_rx_tick(&bumps, millis()));  // a lost STOP still releases
      const unsigned long now = millis();
      if (now - lastHeartbeat >= ANCHOR_HEARTBEAT_MS) {
        lastHeartbeat = now;
        sendHeartbeat();
      }
      if (now - lastTelemetry >= TELEMETRY_MS) {
        lastTelemetry = now;
        sendTelemetry();
      }
      if (audio && now - lastAudio >= AUDIO_MS) {
        lastAudio = now;
        sendAudio();
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
        char buf[80];
        snprintf(buf, sizeof(buf), "ch %d%s, rx %lu, cmds %lu, bumps %lu, audio %s", WiFi.channel(),
                 anchor ? " (anchor)" : "", (unsigned long)rxCount, (unsigned long)cmdCount, (unsigned long)bumpCount,
                 !audio ? "off" : audioSent ? "sending" : "no AudioReactive");
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
      top[F("audio")] = audio;
    }

    bool readFromConfig(JsonObject &root) override {
      JsonObject top = root[FPSTR(_name)];
      bool complete = !top.isNull();
      complete &= getJsonValue(top[F("enabled")], enabled, true);
      complete &= getJsonValue(top[F("anchor")], anchor, true);
      complete &= getJsonValue(top[F("audio")], audio, false);
      return complete;
    }

    uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};

const char NibblesUsermod::_name[] PROGMEM = "Nibbles";

static NibblesUsermod nibbles_usermod;
REGISTER_USERMOD(nibbles_usermod);
