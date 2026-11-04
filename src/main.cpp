// POC 3: teste de alcance e estabilidade do FM-504 (protocolo de texto, 38400 baud).
// Fiacao: GPIO16 = RX do ESP (<- TX do modulo), GPIO17 = TX do ESP (-> RX do modulo).
//
// Comandos (digite no monitor serial e aperte Enter):
//   help            lista os comandos
//   info            mostra versao, potencia e regiao atuais
//   power XX        define a potencia RF, XX em hex (00..FF), ex.: power C0
//   region N        define a regiao, N decimal (1=US, 5=EU), ex.: region 1
//   test [N] [gapMs] faz N tentativas (padrao 50) com gapMs de pausa entre elas (padrao 250)
//                    e mostra a taxa de acerto
#include <Arduino.h>

constexpr int RX_PIN = 16;
constexpr int TX_PIN = 17;
constexpr uint32_t BAUD = 38400;
constexpr int DEFAULT_TRIES = 50;
constexpr int DEFAULT_GAP_MS = 250;

HardwareSerial Reader(1);

// Le ate a resposta terminar em "\r\n" ou estourar o tempo maximo.
String readResponse(uint32_t maxMs) {
  String s;
  uint32_t start = millis();
  while (millis() - start < maxMs) {
    while (Reader.available()) s += (char)Reader.read();
    if (s.length() >= 3 && s.endsWith("\r\n")) break;
  }
  return s;
}

String sendCmd(const String& cmd, uint32_t maxMs = 300) {
  while (Reader.available()) Reader.read();  // limpa antes de perguntar
  Reader.print("\n" + cmd + "\r");
  return readResponse(maxMs);
}

void printEscaped(const String& s) {
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '\n') Serial.print("\\n");
    else if (c == '\r') Serial.print("\\r");
    else if (c >= 32 && c < 127) Serial.print(c);
    else Serial.printf("\\x%02X", (uint8_t)c);
  }
}

// Resposta de tag: Q<PC 4 hex><EPC N hex><CRC16 4 hex>. Devolve o EPC ou "".
String parseEpc(const String& raw) {
  String t = raw;
  t.trim();
  if (t.startsWith("Q") && t.length() > 9) {
    String payload = t.substring(1);
    return payload.substring(4, payload.length() - 4);
  }
  return "";
}

void printHelp() {
  Serial.println("Comandos: help | info | power XX (hex) | region N (1=US, 5=EU) | test [N] [gapMs] [delaySec] | scan [runs] [delaySec] | watch [N] | raw CMD");
}

void cmdInfo() {
  Serial.print("Versao  : "); printEscaped(sendCmd("V")); Serial.println();
  Serial.print("Potencia: "); printEscaped(sendCmd("N0,00")); Serial.println();
  Serial.print("Regiao  : "); printEscaped(sendCmd("N4,00")); Serial.println();
}

void cmdPower(const String& arg) {
  String hex = arg;
  hex.trim();
  hex.toUpperCase();
  if (hex.length() != 2) { Serial.println("Uso: power XX (2 digitos hex, ex.: C0)"); return; }
  Serial.print("Enviado N1,"); Serial.print(hex); Serial.print(" -> ");
  printEscaped(sendCmd("N1," + hex)); Serial.println();
}

void cmdRegion(const String& arg) {
  int n = arg.toInt();
  if (n < 1 || n > 10) { Serial.println("Uso: region N (1 a 10; 1=US, 5=EU)"); return; }
  char buf[8];
  snprintf(buf, sizeof(buf), "N5,%02X", n);
  Serial.printf("Enviado %s -> ", buf);
  printEscaped(sendCmd(buf)); Serial.println();
}

void cmdTest(int tries, int gapMs, int delaySec) {
  for (int s = delaySec; s > 0; s--) {
    Serial.printf("Comecando em %d s... posicione a tag\n", s);
    delay(1000);
  }
  Serial.printf("Testando %d leituras, intervalo %d ms (mantenha a tag parada)...\n", tries, gapMs);
  int hits = 0;
  String firstEpc;
  int otherEpc = 0;
  uint32_t t0 = millis();
  for (int i = 0; i < tries; i++) {
    String epc = parseEpc(sendCmd("Q"));
    if (epc.length() > 0) {
      hits++;
      if (firstEpc.length() == 0) firstEpc = epc;
      else if (epc != firstEpc) otherEpc++;
    }
    delay(gapMs);
  }
  uint32_t ms = millis() - t0;
  Serial.printf("Resultado: %d/%d acertos (%d%%) em %u ms\n", hits, tries, hits * 100 / tries, (unsigned)ms);
  if (firstEpc.length()) Serial.printf("EPC: %s\n", firstEpc.c_str());
  if (otherEpc) Serial.printf("Atencao: %d leituras com EPC diferente do primeiro\n", otherEpc);
}

// Simula o scan do app: cada scan tenta ler por ate 3 s e para na primeira tag valida.
// Uso: scan [runs] [delaySec]  (padrao: 20 scans, 0 s de contagem antes de comecar)
void cmdScan(int runs, int delaySec) {
  constexpr uint32_t WINDOW_MS = 3000;
  constexpr uint32_t GAP_MS = 250;
  constexpr uint32_t PAUSE_BETWEEN_SCANS_MS = 1000;

  for (int s = delaySec; s > 0; s--) {
    Serial.printf("Comecando em %d s... posicione a tag\n", s);
    delay(1000);
  }
  Serial.printf("Scan: %d tentativas, janela de %u ms cada (mantenha a tag parada)\n", runs, (unsigned)WINDOW_MS);

  int ok = 0;
  uint32_t totalOkMs = 0;
  for (int i = 1; i <= runs; i++) {
    uint32_t start = millis();
    String epc;
    while (millis() - start < WINDOW_MS) {
      epc = parseEpc(sendCmd("Q"));
      if (epc.length() > 0) break;
      delay(GAP_MS);
    }
    uint32_t elapsed = millis() - start;
    if (epc.length() > 0) {
      ok++;
      totalOkMs += elapsed;
      Serial.printf("  #%d: OK em %u ms\n", i, (unsigned)elapsed);
    } else {
      Serial.printf("  #%d: FALHOU (%u ms)\n", i, (unsigned)elapsed);
    }
    delay(PAUSE_BETWEEN_SCANS_MS);
  }
  Serial.printf("Resultado: %d/%d scans com sucesso (%d%%)", ok, runs, ok * 100 / runs);
  if (ok > 0) Serial.printf(", tempo medio ate ler: %u ms", (unsigned)(totalOkMs / ok));
  Serial.println();
}

// Replica o comportamento da POC 2 (que leu tags): espera fixa de 300 ms, pausa de 500 ms.
void cmdWatch(int n) {
  Serial.printf("Watch: %d leituras, janela fixa de 300 ms, pausa de 500 ms\n", n);
  for (int i = 0; i < n; i++) {
    while (Reader.available()) Reader.read();
    Reader.print("\nQ\r");
    String raw;
    uint32_t start = millis();
    while (millis() - start < 300) {
      while (Reader.available()) raw += (char)Reader.read();
    }
    Serial.print("Q -> ");
    printEscaped(raw);
    Serial.println();
    delay(500);
  }
}

// Envia um comando qualquer e mostra a resposta crua (janela fixa de 400 ms).
void cmdRaw(const String& arg) {
  if (arg.length() == 0) { Serial.println("Uso: raw CMD (ex.: raw U)"); return; }
  while (Reader.available()) Reader.read();
  Reader.print("\n" + arg + "\r");
  String raw;
  uint32_t start = millis();
  while (millis() - start < 400) {
    while (Reader.available()) raw += (char)Reader.read();
  }
  Serial.print(arg + " -> ");
  printEscaped(raw);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== POC RFID: alcance e estabilidade ===");
  Reader.begin(BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(100);
  cmdInfo();
  printHelp();
}

void loop() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  int sp = line.indexOf(' ');
  String cmd = sp < 0 ? line : line.substring(0, sp);
  String arg = sp < 0 ? "" : line.substring(sp + 1);
  cmd.toLowerCase();

  if (cmd == "help") printHelp();
  else if (cmd == "info") cmdInfo();
  else if (cmd == "power") cmdPower(arg);
  else if (cmd == "region") cmdRegion(arg);
  else if (cmd == "scan") {
    // scan [runs] [delaySec]
    int runs = 20, delaySec = 0;
    if (arg.length()) {
      runs = arg.toInt();
      int s2 = arg.indexOf(' ');
      if (s2 >= 0) delaySec = arg.substring(s2 + 1).toInt();
    }
    if (runs <= 0) runs = 20;
    if (delaySec < 0) delaySec = 0;
    cmdScan(runs, delaySec);
  }
  else if (cmd == "watch") cmdWatch(arg.length() && arg.toInt() > 0 ? arg.toInt() : 20);
  else if (cmd == "raw") cmdRaw(arg);
  else if (cmd == "test") {
    // test [N] [gapMs] [delaySec]
    int n = DEFAULT_TRIES, gap = DEFAULT_GAP_MS, delaySec = 0;
    if (arg.length()) {
      n = arg.toInt();
      int s2 = arg.indexOf(' ');
      if (s2 >= 0) {
        String rest = arg.substring(s2 + 1);
        rest.trim();
        gap = rest.toInt();
        int s3 = rest.indexOf(' ');
        if (s3 >= 0) delaySec = rest.substring(s3 + 1).toInt();
      }
    }
    if (n <= 0) n = DEFAULT_TRIES;
    if (gap < 0) gap = DEFAULT_GAP_MS;
    if (delaySec < 0) delaySec = 0;
    cmdTest(n, gap, delaySec);
  }
  else Serial.println("Comando desconhecido. Digite help.");
}
