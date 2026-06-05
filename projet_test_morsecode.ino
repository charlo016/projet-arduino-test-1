/*
  ESP32 WiFi Morse Code Decoder
  Target: Freenove ESP32-S3-WROOM

  Creates a WiFi access point, hosts a web interface that displays
  real-time Morse code input decoded from the BOOT button.

  - Short press (<300ms) : dot   — LED flickers
  - Long press  (>=300ms): dash  — LED stays on until release
  - 1.5s silence          : end of letter, auto-decode
  - 3.5s silence          : word gap (space added)
*/

#include <WiFi.h>
#include <WebServer.h>

// ========================
//  PIN DEFINITIONS
// ========================
#define LED_PIN    2   // Onboard LED (change to 48 if your board uses NeoPixel)
#define BUTTON_PIN 0   // BOOT button

// ========================
//  NETWORK
// ========================
const char* AP_SSID = "ESP32-Morse";
const char* AP_PASS = "12345678";

// ========================
//  MORSE TIMING (ms)
// ========================
const unsigned long DASH_THRESHOLD = 300;   // below = dot, at or above = dash
const unsigned long CHAR_GAP       = 1500;  // silence to end a character
const unsigned long WORD_GAP       = 3500;  // silence to start a new word

// ========================
//  MORSE LOOKUP TABLE
// ========================
const struct { const char* code; char ch; } MORSE[] = {
  {".-",'A'},{"-...",'B'},{"-.-.",'C'},{"-..",'D'},{".",'E'},
  {"..-.",'F'},{"--.",'G'},{"....",'H'},{"..",'I'},{".---",'J'},
  {"-.-",'K'},{".-..",'L'},{"--",'M'},{"-.",'N'},{"---",'O'},
  {".--.",'P'},{"--.-",'Q'},{".-.",'R'},{"...",'S'},{"-",'T'},
  {"..-",'U'},{"...-",'V'},{".--",'W'},{"-..-",'X'},{"-.--",'Y'},
  {"--..",'Z'},{"-----",'0'},{".----",'1'},{"..---",'2'},
  {"...--",'3'},{"....-",'4'},{".....",'5'},{"-....", '6'},
  {"--...", '7'},{"---..", '8'},{"----.", '9'}
};

char decodeMorse(const String& code) {
  for (const auto& e : MORSE) {
    if (code.equals(e.code)) return e.ch;
  }
  return '?';
}

// ========================
//  STATE
// ========================
String currentMorse  = "";  // symbol sequence for the character being typed
String decodedText   = "";  // full decoded message

bool          buttonWasPressed = false;
unsigned long pressStart       = 0;
unsigned long lastRelease      = 0;

// ========================
//  WEB SERVER
// ========================
WebServer server(80);

const char HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Morse ESP32</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:Arial,sans-serif;background:#0d1117;color:#c9d1d9;min-height:100vh;padding:20px}
  h1{text-align:center;color:#58a6ff;margin-bottom:20px;font-size:1.5em}
  .card{background:#161b22;border:1px solid #30363d;border-radius:12px;padding:16px;margin-bottom:16px}
  .label{font-size:.75em;color:#8b949e;text-transform:uppercase;letter-spacing:1px;margin-bottom:8px}
  .morse-display{font-size:2.2em;letter-spacing:6px;min-height:52px;color:#f0883e;font-family:monospace}
  .text-display{font-size:1.6em;min-height:52px;color:#3fb950;font-family:monospace;word-break:break-all;white-space:pre-wrap}
  .btn-row{display:flex;gap:10px;justify-content:center}
  button{background:#21262d;border:1px solid #30363d;color:#c9d1d9;padding:10px 28px;border-radius:8px;cursor:pointer;font-size:.95em;transition:.2s}
  button:hover{background:#30363d;border-color:#58a6ff;color:#58a6ff}
  .guide{font-size:.82em;color:#8b949e;line-height:2}
  .dot{color:#3fb950}.dash{color:#f0883e}
  #status{text-align:center;font-size:.75em;color:#8b949e;margin-top:10px}
</style>
</head>
<body>
<h1>&#128225; ESP32 Morse Code</h1>

<div class="card">
  <div class="label">Lettre en cours</div>
  <div class="morse-display" id="cur">&#8212;</div>
</div>

<div class="card">
  <div class="label">Message d&eacute;cod&eacute;</div>
  <div class="text-display" id="msg">En attente...</div>
</div>

<div class="card btn-row">
  <button onclick="req('/clear')">&#128465; Effacer</button>
  <button onclick="req('/backspace')">&#9003; Retour</button>
  <button onclick="req('/space')">&#9251; Espace</button>
</div>

<div class="card guide">
  <b>Guide :</b><br>
  Appui court (&lt;300ms) = <span class="dot">&#9679; point</span><br>
  Appui long (&ge;300ms) = <span class="dash">&#8212; tiret</span><br>
  Silence 1.5s = fin de lettre<br>
  Silence 3.5s = nouveau mot
</div>

<div id="status">Connexion...</div>

<script>
function fmt(s){
  return s.replace(/\./g,'<span class="dot">&#9679;</span> ')
          .replace(/-/g,'<span class="dash">&#8212;</span> ');
}
function req(url){fetch(url);}
function poll(){
  fetch('/state').then(r=>r.json()).then(d=>{
    document.getElementById('cur').innerHTML = d.cur ? fmt(d.cur) : '&#8212;';
    document.getElementById('msg').innerText = d.msg || 'En attente...';
    document.getElementById('status').innerText = 'Connect&#233; &#10003; | ' + new Date().toLocaleTimeString();
  }).catch(()=>{
    document.getElementById('status').innerText = 'D&#233;connect&#233;...';
  });
}
setInterval(poll, 250);
poll();
</script>
</body>
</html>
)rawhtml";

void onRoot()      { server.send_P(200, "text/html", HTML); }

void onState() {
  String json = "{\"cur\":\"" + currentMorse + "\",\"msg\":\"" + decodedText + "\"}";
  server.send(200, "application/json", json);
}

void onClear() {
  currentMorse = "";
  decodedText  = "";
  server.send(200, "text/plain", "ok");
}

void onBackspace() {
  if (decodedText.length() > 0)
    decodedText.remove(decodedText.length() - 1);
  server.send(200, "text/plain", "ok");
}

void onSpace() {
  if (decodedText.length() > 0 && decodedText.charAt(decodedText.length() - 1) != ' ')
    decodedText += ' ';
  server.send(200, "text/plain", "ok");
}

// ========================
//  SETUP
// ========================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();

  Serial.println(F("\n================================"));
  Serial.println(F("   ESP32 - Morse Code Decoder"));
  Serial.println(F("================================"));
  Serial.print(F("SSID      : ")); Serial.println(AP_SSID);
  Serial.print(F("Mot passe : ")); Serial.println(AP_PASS);
  Serial.print(F("URL       : http://")); Serial.println(ip);
  Serial.println(F("================================"));
  Serial.println(F("Court (<300ms) = point   Long (>=300ms) = tiret\n"));

  server.on("/",          onRoot);
  server.on("/state",     onState);
  server.on("/clear",     onClear);
  server.on("/backspace", onBackspace);
  server.on("/space",     onSpace);
  server.begin();
}

// ========================
//  LOOP
// ========================
void loop() {
  server.handleClient();

  bool pressed = (digitalRead(BUTTON_PIN) == LOW);
  unsigned long now = millis();

  // Button just pressed
  if (pressed && !buttonWasPressed) {
    pressStart = now;
    digitalWrite(LED_PIN, HIGH);
  }

  // Button just released
  if (!pressed && buttonWasPressed) {
    unsigned long dur = now - pressStart;
    digitalWrite(LED_PIN, LOW);
    lastRelease = now;

    if (dur < DASH_THRESHOLD) {
      currentMorse += '.';
      Serial.print(F(". "));
    } else {
      currentMorse += '-';
      Serial.print(F("- "));
    }
  }

  // Check for letter/word gap while idle
  if (!pressed && currentMorse.length() > 0 && lastRelease > 0) {
    unsigned long gap = now - lastRelease;

    if (gap >= WORD_GAP) {
      char c = decodeMorse(currentMorse);
      decodedText += c;
      decodedText += ' ';
      Serial.println();
      Serial.print(F(">> Word: ")); Serial.println(decodedText);
      currentMorse = "";
      lastRelease  = 0;

    } else if (gap >= CHAR_GAP) {
      char c = decodeMorse(currentMorse);
      decodedText += c;
      Serial.println();
      Serial.print(F(">> Char: ")); Serial.println(String(c));
      Serial.print(F(">> Text: ")); Serial.println(decodedText);
      currentMorse = "";
      lastRelease  = 0;
    }
  }

  buttonWasPressed = pressed;
}
