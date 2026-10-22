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
// - 2D map partners: with the shark's 2D map (/ledmap.json, made by
//   tools/sim/make_ledmap.py) only one of each overlapping pair is mapped
//   (the outward body strip, the starboard fin); after every frame the LEDs
//   in the file's "copy" list take the colour of the LED they pair with.
// - Battery-aware current limit: the base broadcasts the pack voltage at the
//   pole base (NL_MSG_BATTERY); WLED's global current limit follows it so a
//   full-white flash can't pull the 5 V converter's input below the voltage
//   where it stutters (nl_power_limit_ma; docs/power.md). The ceiling is this
//   usermod's power_max_ma setting, which takes over WLED's own limit field.
// - Fin sweep for testing (the simulator asks, {"nibbles":{"finsweep":{"port":
//   period_s,"starboard":period_s,"s":seconds}}}): the fin's angle swings
//   smoothly through -90..+90 deg, in place of the sensor, until the request
//   runs out (renewed every 2 s while the simulator wants it).
// - Fin hinge angles from the fin sensor node (NL_MSG_FIN_POSE): shown on the
//   Info page and passed to the simulator. With the 2D map, a fin whose angle
//   is known follows it: each of its LEDs shows the side view's colour at the
//   spot it is really over ("fingeo" in /ledmap.json, from make_ledmap.py).
// - Mirror for the lighting simulator (tools/sim): while asked to (JSON state
//   {"nibbles":{"mirror":{"ip":..,"port":..,"s":..}}}, renewed every few
//   seconds), streams every LED as it is shown (DDP, to port) and the eyes'
//   audio features and telemetry (JSON text, to port + 1).
//
// Messages come from shared/nibbles_link (the same protocol code as the eyes
// and base). See docs/architecture.md in the Nibbles repo.
#include "wled.h"
#include "nibbles_link.h"
#include <esp_now.h>
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
#define MIRROR_MIN_MS          20     // at most 50 frames a second to the simulator
#define MIRROR_MAX_S           30     // a mirror request lasts at most this long
#define DDP_MAX_LEDS           480    // LEDs per DDP packet (1440 bytes of RGB)

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
    int pendingPreset = 0, pendingBri = -1;  // commands received during a bump

    // Mirror to the simulator.
    WiFiUDP mirrorUdp;
    IPAddress mirrorIp;
    uint16_t mirrorPort = 0;
    unsigned long mirrorUntil = 0, lastMirror = 0;
    uint8_t ddpSeq = 0;
    uint32_t mirrorFrames = 0;
    nl_audio_t lastAudioOut = {};
    unsigned long lastAudioOutAt = 0;
    uint8_t ddpBuf[10 + DDP_MAX_LEDS * 3];
    uint8_t *mirrorRgb = nullptr;  // one frame by physical LED (in 2D, cells are not LEDs)
    unsigned mirrorRgbLeds = 0;

    // 2D map partners: "copy" ranges from /ledmap.json ([dst, dst count,
    // src, src count], physical LED numbers), turned into matrix cell pairs.
    struct CopyRange { uint16_t dst, dstN, src, srcN; };
    CopyRange copyRanges[4];
    uint8_t copyRangeCount = 0;
    bool copyRead = false;
    uint16_t *copyPairs = nullptr;  // cell to paint, cell to copy from, ...
    uint16_t copyPairCount = 0, copyBuiltFor = 0;
    int copyBuiltMap = -1;
    bool copyBuiltMatrix = false;
    unsigned long copyBuiltAt = 0;

    // Fin hinge angles, as last heard.
    nl_fin_pose_t fins = { { NL_FIN_NONE, NL_FIN_NONE }, { NL_FIN_RAW_NONE, NL_FIN_RAW_NONE } };
    unsigned long finsAt[2] = { 0, 0 };
    uint32_t finRx = 0;

    // Battery-aware current limit.
    bool powerEnabled = true;
    nl_power_model_t powerModel = NL_POWER_MODEL_DEFAULT;
    nl_power_limiter_t power;
    uint32_t powerApplied = 0;    // limit we last set, 0 = not yet (WLED's own until then)
    unsigned long lastPowerTick = 0;
    uint32_t batteryRx = 0;

    static const char _name[];

    // Keep WLED's global limit at the limiter's value (or the ceiling while no
    // battery reading has arrived). Leaves WLED alone when its limiter is off.
    void powerLoop(unsigned long now) {
      if (now - lastPowerTick < 100) return;
      lastPowerTick = now;
      const uint32_t lim = nl_power_limiter_tick(&power, now);
      if (!powerEnabled || BusManager::ablMilliampsMax() == 0) return;
      const uint32_t want = lim ? lim : powerModel.max_ma;
      if (want != powerApplied || BusManager::ablMilliampsMax() != want) {
        BusManager::setMilliampsMax(want > 65535 ? 65535 : want);
        powerApplied = want;
      }
    }

    void batteryReading(const nl_battery_t &b) {
      batteryRx++;
      nl_power_limiter_reading(&power, &b, millis());
    }

    // Test sweep of the fins (from the simulator), in place of the sensor.
    float finSweepPeriod[2] = { 0, 0 };   // seconds per full swing, 0 = off
    unsigned long finSweepUntil = 0, finSweepStart = 0;

    bool finSweeping(int side, unsigned long now) const {
      return finSweepPeriod[side] > 0 && (long)(finSweepUntil - now) > 0;
    }

    // The angle a fin has now: the test sweep, else a fresh calibrated sensor reading.
    bool finAngle(int side, unsigned long now, float &deg) const {
      if (finSweeping(side, now)) {
        deg = 90.0f * sinf(2.0f * (float)M_PI * ((now - finSweepStart) / 1000.0f) / finSweepPeriod[side]);
        return true;
      }
      if (!finsAt[side] || now - finsAt[side] > 2000 || fins.angle_cdeg[side] == NL_FIN_NONE) return false;
      deg = fins.angle_cdeg[side] / 100.0f;
      return true;
    }

    // Fins following their real angle in 2D ("fingeo" in /ledmap.json).
    struct FinFollow {
      uint8_t side;               // NL_FIN_PORT / NL_FIN_STARBOARD
      uint16_t start, count;      // physical LEDs
      float hinge[3], turn[3], modelDeg;
      float *rest;                // count x (x, y, z), design mm
      uint16_t *dst;              // the matrix cell that drives each LED
      uint16_t *src;              // the cell each LED is over at srcDeg
      float srcDeg;               // NAN: not worked out yet
    };
    FinFollow finFollow[2] = {};
    uint8_t finFollowCount = 0;
    float finGrid[5] = { 0 };     // cell size, u0, v1, ox, oy (see make_ledmap.py)
    uint32_t *finColors = nullptr;
    uint16_t finColorsLen = 0;
    uint8_t finFollowing = 0;     // bit per fin following its angle on the last frame

    // One number from a JSON array; false at its closing bracket or the end.
    static bool nextNumber(File &f, float &out) {
      char buf[24];
      int n = 0;
      while (f.available()) {
        const char c = f.read();
        if ((c >= '0' && c <= '9') || c == '-' || c == '.' || c == 'e' || c == 'E' || c == '+') {
          if (n < (int)sizeof(buf) - 1) buf[n++] = c;
          continue;
        }
        if (n) { buf[n] = 0; out = strtof(buf, nullptr); return true; }
        if (c == ']') return false;
      }
      return false;
    }

    void freeFinFollow() {
      for (int k = 0; k < 2; k++) {
        free(finFollow[k].rest); free(finFollow[k].dst); free(finFollow[k].src);
        finFollow[k] = {};
      }
      finFollowCount = 0;
      free(finColors); finColors = nullptr; finColorsLen = 0;
    }

    void readFinGeo() {
      freeFinFollow();
      File f = WLED_FS.open(F("/ledmap.json"), "r");
      if (!f) return;
      if (f.find("\"fingeo\":[")) {
        bool ok = true;
        for (int i = 0; i < 5 && ok; i++) ok = nextNumber(f, finGrid[i]);
        float nf = 0;
        ok = ok && nextNumber(f, nf) && nf >= 0 && nf <= 2;
        const int n = ok ? (int)nf : 0;
        for (int k = 0; k < n && ok; k++) {
          FinFollow &ff = finFollow[k];
          float side = 0, start = 0, count = 0;
          ok = nextNumber(f, side) && nextNumber(f, start) && nextNumber(f, count);
          for (int j = 0; j < 3 && ok; j++) ok = nextNumber(f, ff.hinge[j]);
          for (int j = 0; j < 3 && ok; j++) ok = nextNumber(f, ff.turn[j]);
          ok = ok && nextNumber(f, ff.modelDeg) && count > 0 && count < 1000;
          ff.side = (uint8_t)side; ff.start = (uint16_t)start; ff.count = (uint16_t)count;
          ff.srcDeg = NAN;
        }
        for (int k = 0; k < n && ok; k++) {
          FinFollow &ff = finFollow[k];
          ff.rest = (float *)malloc(ff.count * 3 * sizeof(float));
          ok = ff.rest != nullptr;
          for (unsigned i = 0; i < ff.count * 3u && ok; i++) ok = nextNumber(f, ff.rest[i]);
        }
        if (ok) finFollowCount = n;
        else freeFinFollow();
      }
      f.close();
    }

    // Swing a fin to deg and find the cell each of its LEDs is over now.
    void placeFin(FinFollow &f, float deg, unsigned W, unsigned H) {
      const float a = (f.modelDeg - deg) * (float)M_PI / 180.0f, cs = cosf(a), sn = sinf(a);
      const float *k = f.turn, *c = f.hinge;
      const float cell = finGrid[0], u0 = finGrid[1], v1 = finGrid[2], ox = finGrid[3], oy = finGrid[4];
      for (unsigned i = 0; i < f.count; i++) {
        const float *p = f.rest + i * 3;
        const float v[3] = { p[0] - c[0], p[1] - c[1], p[2] - c[2] };
        const float kv = k[0] * v[0] + k[1] * v[1] + k[2] * v[2];
        const float kx[3] = { k[1] * v[2] - k[2] * v[1], k[2] * v[0] - k[0] * v[2], k[0] * v[1] - k[1] * v[0] };
        float q[3];
        for (int j = 0; j < 3; j++) q[j] = c[j] + v[j] * cs + kx[j] * sn + k[j] * kv * (1 - cs);
        long x = lroundf((q[1] - u0 + ox) / cell - 0.5f), y = lroundf((v1 - q[2] + oy) / cell - 0.5f);
        x = x < 0 ? 0 : x >= (long)W ? (long)W - 1 : x;
        y = y < 0 ? 0 : y >= (long)H ? (long)H - 1 : y;
        f.src[i] = (uint16_t)(y * W + x);
      }
      f.srcDeg = deg;
    }

    // Fins whose angle is known: gather the colours under each LED (from the
    // effect's own picture, before anything is copied over it).
    void gatherFinColors() {
      finFollowing = 0;
      if (!finFollowCount || !finColors) return;
      const unsigned long now = millis();
      const unsigned W = Segment::maxWidth, H = Segment::maxHeight;
      unsigned n = 0;
      for (int k = 0; k < finFollowCount; k++) {
        FinFollow &f = finFollow[k];
        float deg;
        if (!f.dst || !f.src || f.side > 1 || !finAngle(f.side, now, deg)) continue;
        if (isnan(f.srcDeg) || fabsf(deg - f.srcDeg) >= 0.2f) placeFin(f, deg, W, H);
        // NoMap: most cells have no LED of their own, and getPixelColor() reads those as black.
        for (unsigned i = 0; i < f.count && n < finColorsLen; i++) finColors[n++] = strip.getPixelColorNoMap(f.src[i]);
        finFollowing |= 1 << k;
      }
    }

    void writeFinColors() {
      unsigned n = 0;
      for (int k = 0; k < finFollowCount; k++) {
        if (!(finFollowing & (1 << k))) continue;
        const FinFollow &f = finFollow[k];
        for (unsigned i = 0; i < f.count && n < finColorsLen; i++, n++)
          if (f.dst[i] != 0xFFFF) strip.setPixelColor(f.dst[i], finColors[n]);
      }
    }

    void readCopyRanges() {
      copyRangeCount = 0;
      File f = WLED_FS.open(F("/ledmap.json"), "r");
      if (!f) return;
      if (f.find("\"copy\":[")) {
        uint32_t vals[16];
        int n = 0, depth = 1;
        long num = -1;
        while (f.available() && depth > 0 && n < 16) {
          const char c = f.read();
          if (c >= '0' && c <= '9') { num = (num < 0 ? 0 : num * 10) + (c - '0'); continue; }
          if (num >= 0) { vals[n++] = num; num = -1; }
          if (c == '[') depth++;
          else if (c == ']') depth--;
        }
        for (int i = 0; i + 3 < n && copyRangeCount < 4; i += 4)
          if (vals[i + 1] && vals[i + 3]) copyRanges[copyRangeCount++] = { (uint16_t)vals[i], (uint16_t)vals[i + 1], (uint16_t)vals[i + 2], (uint16_t)vals[i + 3] };
      }
      f.close();
    }

    // Matrix cell of each physical LED, from WLED's map; then the pairs.
    void buildCopyPairs() {
      free(copyPairs);
      copyPairs = nullptr;
      copyPairCount = 0;
      copyBuiltFor = strip.getLengthTotal();
      copyBuiltMap = currentLedmap;
      copyBuiltMatrix = strip.isMatrix;
      copyBuiltAt = millis();
      // every output, network ones included (the simulator bench sends its LEDs over the network)
      const unsigned phys = BusManager::getTotalLength(false), total = strip.getLengthTotal();
      if (!strip.isMatrix || (!copyRangeCount && !finFollowCount) || !phys) return;
      uint16_t *cellOf = (uint16_t *)malloc(phys * sizeof(uint16_t));
      if (!cellOf) return;
      for (unsigned p = 0; p < phys; p++) cellOf[p] = 0xFFFF;
      for (unsigned i = 0; i < total; i++) {
        const uint16_t p = strip.getMappedPixelIndex(i);
        if (p < phys) cellOf[p] = i;
      }
      unsigned count = 0;
      for (int r = 0; r < copyRangeCount; r++) count += copyRanges[r].dstN;
      copyPairs = (uint16_t *)malloc(count * 2 * sizeof(uint16_t));
      if (copyPairs) {
        for (int r = 0; r < copyRangeCount; r++) {
          const CopyRange &c = copyRanges[r];
          for (unsigned k = 0; k < c.dstN; k++) {
            // the partner at the same fraction along its own range
            const unsigned d = c.dst + k, sidx = c.src + (c.dstN > 1 ? (k * (c.srcN - 1) + (c.dstN - 1) / 2) / (c.dstN - 1) : 0);
            if (d >= phys || sidx >= phys || cellOf[d] == 0xFFFF || cellOf[sidx] == 0xFFFF) continue;
            copyPairs[copyPairCount * 2] = cellOf[d];
            copyPairs[copyPairCount * 2 + 1] = cellOf[sidx];
            copyPairCount++;
          }
        }
      }
      // The cells that drive each fin LED, for following the fins' angle.
      unsigned finLeds = 0;
      for (int k = 0; k < finFollowCount; k++) {
        FinFollow &f = finFollow[k];
        free(f.dst); free(f.src);
        f.dst = (uint16_t *)malloc(f.count * sizeof(uint16_t));
        f.src = (uint16_t *)malloc(f.count * sizeof(uint16_t));
        f.srcDeg = NAN;
        if (!f.dst || !f.src) { free(f.dst); free(f.src); f.dst = f.src = nullptr; continue; }
        for (unsigned i = 0; i < f.count; i++) f.dst[i] = f.start + i < phys ? cellOf[f.start + i] : 0xFFFF;
        finLeds += f.count;
      }
      free(finColors);
      finColors = finLeds ? (uint32_t *)malloc(finLeds * sizeof(uint32_t)) : nullptr;
      finColorsLen = finColors ? finLeds : 0;
      free(cellOf);
    }

    bool mirroring(unsigned long now) const { return mirrorPort && (long)(mirrorUntil - now) > 0; }

    // One frame as the LEDs show it (gamma and brightness applied; the
    // current limiter's extra dimming is not included), as DDP packets.
    void sendMirrorFrame() {
      // Every LED's colour, put where WLED sends it (through the LED map, so
      // 2D matrix cells land on their LEDs), with gamma and brightness.
      const unsigned phys = BusManager::getTotalLength(false), total = strip.getLengthTotal();
      if (mirrorRgbLeds != phys) {
        free(mirrorRgb);
        mirrorRgb = (uint8_t *)malloc(phys * 3);
        mirrorRgbLeds = mirrorRgb ? phys : 0;
        if (!mirrorRgb) return;
      }
      memset(mirrorRgb, 0, phys * 3);
      const uint8_t bri = strip.getBrightness();
      if (bri) {
        for (unsigned i = 0; i < total; i++) {
          const unsigned p = strip.getMappedPixelIndex(i);
          if (p >= phys) continue;
          uint32_t c = strip.getPixelColor(i);
          if (!c) continue;
          c = gamma32(c);
          const uint8_t w = W(c);
          uint8_t *d = mirrorRgb + p * 3;
          d[0] = scale8(qadd8(w, R(c)), bri);  // white folded into RGB, like the live view
          d[1] = scale8(qadd8(w, G(c)), bri);
          d[2] = scale8(qadd8(w, B(c)), bri);
        }
      }
      for (unsigned start = 0; start < phys; start += DDP_MAX_LEDS) {
        const unsigned n = min((unsigned)DDP_MAX_LEDS, phys - start);
        const bool last = start + n >= phys;
        const uint32_t offset = start * 3;
        ddpBuf[0] = 0x40 | (last ? 0x01 : 0x00);  // version 1, push on the last packet
        ddpBuf[1] = ddpSeq & 0x0f;
        ddpBuf[2] = 0x0b;                          // RGB, 8 bits per channel
        ddpBuf[3] = 1;                             // output device
        ddpBuf[4] = offset >> 24; ddpBuf[5] = offset >> 16; ddpBuf[6] = offset >> 8; ddpBuf[7] = offset;
        ddpBuf[8] = (n * 3) >> 8; ddpBuf[9] = (n * 3) & 0xff;
        memcpy(ddpBuf + 10, mirrorRgb + offset, n * 3);
        mirrorUdp.beginPacket(mirrorIp, mirrorPort);
        mirrorUdp.write(ddpBuf, 10 + n * 3);
        mirrorUdp.endPacket();
      }
      ddpSeq++;
      mirrorFrames++;
    }

    // What the eyes get from us (audio features) and tell us (telemetry), so
    // the simulator's eyes can follow the real ones.
    void sendMirrorState(unsigned long now) {
      char buf[512];
      const nl_audio_t &a = lastAudioOut;
      const nl_eye_telemetry_t &e = eyes;
      int len = snprintf(buf, sizeof(buf),
        "{\"wled\":{\"bri\":%u,\"ps\":%d,\"on\":%s,\"limit_ma\":%u,\"pack_mv\":%u}", bri, currentPreset,
        bri ? "true" : "false", (unsigned)BusManager::ablMilliampsMax(), power.heard ? (unsigned)power.pack_mv : 0u);
      if (lastAudioOutAt && now - lastAudioOutAt < 1000)
        len += snprintf(buf + len, sizeof(buf) - len,
          ",\"audio\":[%.2f,%.2f,%.2f,%.2f,%.3f,%.3f,%.4f,%.3f,%lu,%lu]",
          a.level_db, a.avg_db, a.noise_floor_db, a.gain_db, a.loudness, a.warmth, a.beat_period_s,
          a.beat_confidence, (unsigned long)a.beat_count, (unsigned long)a.peak_count);
      {
        char fbuf[96];
        int fn = 0;
        for (int f = 0; f < 2; f++) {
          const bool fresh = finsAt[f] && now - finsAt[f] < 2000;
          float deg;
          if (finSweeping(f, now) && finAngle(f, now, deg))
            fn += snprintf(fbuf + fn, sizeof(fbuf) - fn, "%s{\"deg\":%.2f,\"sweep\":%.1f}", f ? "," : "", deg, finSweepPeriod[f]);
          else if (!fresh) fn += snprintf(fbuf + fn, sizeof(fbuf) - fn, "%snull", f ? "," : "");
          else if (fins.angle_cdeg[f] == NL_FIN_NONE) fn += snprintf(fbuf + fn, sizeof(fbuf) - fn, "%s{\"raw\":%u}", f ? "," : "", fins.raw[f]);
          else fn += snprintf(fbuf + fn, sizeof(fbuf) - fn, "%s{\"deg\":%.2f,\"raw\":%u}", f ? "," : "", fins.angle_cdeg[f] / 100.0f, fins.raw[f]);
        }
        len += snprintf(buf + len, sizeof(buf) - len, ",\"fins\":[%s]", fbuf);
      }
      if (haveEyes && now - eyesAt < 2000) {
        char name[NL_NAME_LEN + 1] = { 0 };
        for (int i = 0; i < NL_NAME_LEN && e.name[i]; i++) name[i] = (e.name[i] == '"' || e.name[i] == '\\') ? ' ' : e.name[i];
        len += snprintf(buf + len, sizeof(buf) - len,
          ",\"eyes\":{\"preset\":%u,\"count\":%u,\"state\":%u,\"linked\":%u,\"bri\":%u,\"flags\":%u,"
          "\"tempo\":%.1f,\"hype\":%.2f,\"name\":\"%s\"}",
          e.preset, e.preset_count, e.state, e.linked, e.brightness, e.flags, e.tempo_bpm, e.hype, name);
      }
      if (len > 0 && len < (int)sizeof(buf) - 1) {
        buf[len++] = '}';
        mirrorUdp.beginPacket(mirrorIp, mirrorPort + 1);
        mirrorUdp.write((const uint8_t *)buf, len);
        mirrorUdp.endPacket();
      }
    }

    void mirrorLoop() {
      const unsigned long now = millis();
      if (!mirroring(now)) {
        if (mirrorRgb) { free(mirrorRgb); mirrorRgb = nullptr; mirrorRgbLeds = 0; }  // only while streaming
        return;
      }
      if (now - lastMirror < MIRROR_MIN_MS) return;
      lastMirror = now;
      sendMirrorFrame();
      sendMirrorState(now);
    }

    bool radioUp() const { return enabled && enableESPNow && statusESPNow == ESP_NOW_STATE_ON; }

    // Sent straight through ESP-IDF, not quickEspNow.send(): WLED starts
    // QuickEspNow in synchronous mode, where send() spins in WLED's loop until
    // the "sent" callback, and a send that fails (e.g. while Wi-Fi is still
    // connecting in station mode) never gets one. That hung WLED on the shark.
    void send(const uint8_t *mac, uint8_t type, const void *payload, size_t len) {
      uint8_t pkt[NL_RADIO_MAX];
      const size_t n = nl_radio_build(pkt, sizeof(pkt), type, seq++, NL_ROLE_WLED, NL_SIDE_NONE, payload, len);
      if (!n) return;
      esp_now_peer_info_t peer = {};
      memcpy(peer.peer_addr, mac, 6);
      peer.channel = 0;  // the current channel
      peer.ifidx = (apActive && !WLED_CONNECTED) ? WIFI_IF_AP : WIFI_IF_STA;
      esp_now_peer_info_t have;
      if (esp_now_get_peer(mac, &have) != ESP_OK) esp_now_add_peer(&peer);
      else if (have.ifidx != peer.ifidx) esp_now_mod_peer(&peer);
      esp_now_send(mac, pkt, n);  // returns at once; failures are simply dropped
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
      lastAudioOut = a;
      lastAudioOutAt = millis();
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
      // Only the ids and names: a real presets.json (87 KB on the shark) is
      // far bigger than WLED's JSON buffer.
      StaticJsonDocument<64> filter;
      filter["*"]["n"] = true;
      pDoc->clear();
      const DeserializationError err = deserializeJson(*pDoc, f, DeserializationOption::Filter(filter));
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
      // During a bump the saved state comes back on release, which would undo
      // the command: keep it for then instead.
      if (haveSaved) {
        if (id > 0) pendingPreset = id;
        else pendingBri = -id;
        return;
      }
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
      haveSaved = false;
      if (pendingPreset) {
        applyPreset(pendingPreset, CALL_MODE_DIRECT_CHANGE);  // a preset command came in during the bump
      } else {
        // Put everything back instantly.
        String json = savedState;
        if (json.startsWith("{")) json = "{\"tt\":0," + json.substring(1);
        applyJson(json);
        currentPreset = savedPreset;  // restoring the state clears it; it's the same look
      }
      if (pendingBri >= 0) {
        bri = pendingBri;
        stateUpdated(CALL_MODE_DIRECT_CHANGE);
      }
      pendingPreset = 0;
      pendingBri = -1;
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
      } else if (h.type == NL_MSG_FIN_POSE && len == sizeof(nl_fin_pose_t)) {
        nl_fin_pose_t p;
        memcpy(&p, pl, sizeof(p));
        for (int f = 0; f < 2; f++) {
          if (p.raw[f] == NL_FIN_RAW_NONE) continue;  // this node doesn't read that fin
          fins.angle_cdeg[f] = p.angle_cdeg[f];
          fins.raw[f] = p.raw[f];
          finsAt[f] = millis();
        }
        finRx++;
      } else if (h.type == NL_MSG_BATTERY && len == sizeof(nl_battery_t)) {
        nl_battery_t b;
        memcpy(&b, pl, sizeof(b));
        batteryReading(b);
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
      nl_power_limiter_init(&power, &powerModel);
      nl_bump_rx_init(&bumps);
      nl_ar_init(&arState);
      // The shark needs ESP-NOW; switch it on before WLED starts networking.
      if (enabled && !enableESPNow) enableESPNow = true;
    }

    void loop() override {
      if (!copyRead) { readCopyRanges(); readFinGeo(); copyRead = true; }
      // (Re)build when the map changes; WLED loads it after we first run, so
      // also retry every 5 s while nothing pairs up.
      if ((copyRangeCount || finFollowCount) && (strip.getLengthTotal() != copyBuiltFor || currentLedmap != copyBuiltMap ||
                             strip.isMatrix != copyBuiltMatrix || (!copyPairCount && millis() - copyBuiltAt > 5000)))
        buildCopyPairs();
      mirrorLoop();  // on request only, and independent of the radio (works with the usermod disabled)
      powerLoop(millis());  // also without the radio, so it can be tried with test readings
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

    // Called by WLED after the frame is put together, before it goes to the LEDs.
    void handleOverlayDraw() override {
      if (!strip.isMatrix) return;
      gatherFinColors();
      if (copyPairs)
        for (unsigned i = 0; i < copyPairCount; i++)
          strip.setPixelColor(copyPairs[i * 2], strip.getPixelColor(copyPairs[i * 2 + 1]));
      writeFinColors();  // fins following their angle override the partner copies
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
      {
        JsonArray fa = user.createNestedArray(F("Nibbles fins"));
        char buf[96];
        int n = 0;
        static const char *names[2] = { "port", "starboard" };
        for (int f = 0; f < 2; f++) {
          if (finSweeping(f, millis())) n += snprintf(buf + n, sizeof(buf) - n, "%s%s sweeping (%.1f s)", f ? "; " : "", names[f], finSweepPeriod[f]);
          else if (!finsAt[f] || millis() - finsAt[f] > 2000) n += snprintf(buf + n, sizeof(buf) - n, "%s%s not heard", f ? "; " : "", names[f]);
          else if (fins.angle_cdeg[f] == NL_FIN_NONE) n += snprintf(buf + n, sizeof(buf) - n, "%s%s raw %u (uncalibrated)", f ? "; " : "", names[f], fins.raw[f]);
          else n += snprintf(buf + n, sizeof(buf) - n, "%s%s %+.1f deg", f ? "; " : "", names[f], fins.angle_cdeg[f] / 100.0f);
        }
        fa.add(buf);
      }
      {
        JsonArray pw = user.createNestedArray(F("Nibbles power"));
        char buf[80];
        if (!powerEnabled) snprintf(buf, sizeof(buf), "off");
        else if (BusManager::ablMilliampsMax() == 0) snprintf(buf, sizeof(buf), "WLED's current limiter is off");
        else if (!power.heard) snprintf(buf, sizeof(buf), "no battery readings; limit %u mA", (unsigned)BusManager::ablMilliampsMax());
        else snprintf(buf, sizeof(buf), "pack %.2f V; limit %u of %u mA (%lu readings)", power.pack_mv / 1000.0f,
                      (unsigned)BusManager::ablMilliampsMax(), (unsigned)powerModel.max_ma, (unsigned long)batteryRx);
        pw.add(buf);
      }
      if (strip.isMatrix && (copyRangeCount || finFollowCount)) {
        JsonArray m2 = user.createNestedArray(F("Nibbles 2D map"));
        char buf[96];
        snprintf(buf, sizeof(buf), "%u partner LEDs copied; fins following their angle: %s%s%s", copyPairCount,
                 !finFollowCount ? "no fin geometry" : finFollowing ? "" : "none",
                 finFollowing & 1 ? (finFollow[0].side == NL_FIN_PORT ? "port " : "starboard ") : "",
                 finFollowing & 2 ? (finFollow[1].side == NL_FIN_PORT ? "port" : "starboard") : "");
        m2.add(buf);
      }
      if (mirroring(millis())) {
        JsonArray m = user.createNestedArray(F("Nibbles mirror"));
        char buf[48];
        snprintf(buf, sizeof(buf), "to %s:%u, %lu frames", mirrorIp.toString().c_str(), mirrorPort, (unsigned long)mirrorFrames);
        m.add(buf);
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

    // {"nibbles":{"mirror":{"ip":"a.b.c.d","port":4050,"s":5}}}: stream to the
    // simulator for s seconds (0 stops).
    void readFromJsonState(JsonObject &root) override {
      // {"nibbles":{"battery":{"mv":13000,"ma":7700}}}: a test reading, as if
      // from the base ("mv":0 forgets: back to the ceiling).
      // {"nibbles":{"finsweep":{"port":4,"starboard":0,"s":6}}}: sweep test
      JsonObject sw = root[F("nibbles")][F("finsweep")];
      if (!sw.isNull()) {
        const float p0 = sw[F("port")] | 0.0f, p1 = sw[F("starboard")] | 0.0f;
        const int secs = sw[F("s")] | 6;
        if (!finSweeping(0, millis()) && !finSweeping(1, millis())) finSweepStart = millis();  // a fresh sweep starts level
        finSweepPeriod[0] = p0 > 0.2f ? p0 : 0;
        finSweepPeriod[1] = p1 > 0.2f ? p1 : 0;
        finSweepUntil = millis() + (unsigned long)min(max(secs, 0), 30) * 1000UL;
      }
      JsonObject bat = root[F("nibbles")][F("battery")];
      if (!bat.isNull()) {
        nl_battery_t b = {};
        b.pack_mv = bat[F("mv")] | 0;
        if (!bat[F("ma")].isNull()) { b.current_ma = bat[F("ma")] | 0; b.flags = NL_BATT_CURRENT; }
        batteryReading(b);
      }
      JsonObject m = root[F("nibbles")][F("mirror")];
      if (m.isNull()) return;
      IPAddress ip;
      const int secs = m[F("s")] | 0;
      const int port = m[F("port")] | 0;
      if (secs <= 0 || port <= 0 || port > 65534 || !ip.fromString((const char *)(m[F("ip")] | ""))) {
        mirrorPort = 0;
        return;
      }
      mirrorIp = ip;
      mirrorPort = port;
      mirrorUntil = millis() + min(secs, MIRROR_MAX_S) * 1000UL;
    }

    void addToConfig(JsonObject &root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[F("enabled")] = enabled;
      top[F("anchor")] = anchor;
      top[F("audio")] = audio;
      top[F("power")] = powerEnabled;            // battery-aware current limit
      top[F("power_max_ma")] = powerModel.max_ma;  // its ceiling (replaces WLED's own limit field)
      top[F("power_min_ma")] = powerModel.min_ma;
      top[F("floor_mv")] = powerModel.floor_mv;    // lowest input the 5 V converter may see
      top[F("pack_mohm")] = powerModel.pack_mohm;  // pack, BMS, fuse
      top[F("wire_mohm")] = powerModel.wire_mohm;  // pole wiring, there and back
      top[F("conv_eff")] = powerModel.efficiency_pct;
    }

    bool readFromConfig(JsonObject &root) override {
      JsonObject top = root[FPSTR(_name)];
      bool complete = !top.isNull();
      complete &= getJsonValue(top[F("enabled")], enabled, true);
      complete &= getJsonValue(top[F("anchor")], anchor, true);
      complete &= getJsonValue(top[F("audio")], audio, false);
      const nl_power_model_t def = NL_POWER_MODEL_DEFAULT;
      complete &= getJsonValue(top[F("power")], powerEnabled, true);
      complete &= getJsonValue(top[F("power_max_ma")], powerModel.max_ma, def.max_ma);
      complete &= getJsonValue(top[F("power_min_ma")], powerModel.min_ma, def.min_ma);
      complete &= getJsonValue(top[F("floor_mv")], powerModel.floor_mv, def.floor_mv);
      complete &= getJsonValue(top[F("pack_mohm")], powerModel.pack_mohm, def.pack_mohm);
      complete &= getJsonValue(top[F("wire_mohm")], powerModel.wire_mohm, def.wire_mohm);
      complete &= getJsonValue(top[F("conv_eff")], powerModel.efficiency_pct, def.efficiency_pct);
      nl_power_limiter_init(&power, &powerModel);  // new settings: start over (readings arrive several times a second)
      powerApplied = 0;
      return complete;
    }

    uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};

const char NibblesUsermod::_name[] PROGMEM = "Nibbles";

static NibblesUsermod nibbles_usermod;
REGISTER_USERMOD(nibbles_usermod);
