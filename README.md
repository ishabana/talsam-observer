# Talsam observer

LilyGO T-Display-S3 Touch: discover workshop senders, collect public Abjad
records, and offload the collection over USB. No talisman passphrase is present.
Sender firmware is unchanged.

## Play on the board

1. Join the same Wi-Fi as the workshop senders (configured in include/secrets.h).
2. Send a new message from a sender. Its source IP appears in DISCOVER.
3. GPIO14 selects the next IP. GPIO0 or a screen tap collects that IP's records.
4. SQUARES shows the source IP, Abjad total and 4x4 square. GPIO14 browses
   collected records; GPIO0/tap returns to the IP list.
5. Collect again to get newly sent messages. Existing records are deduplicated
   by source and encrypted packet, not by total: identical totals may be distinct messages.

Discovery listens to UDP 4646 traffic delivered to this board, including the
messenger's broadcasts. It does not scan the subnet or capture arbitrary Wi-Fi
unicast traffic. Up to 16 source IPs are tracked with packet/ABJ1 counts and age.
ABJ1 is a format marker, not proof of sender identity. Only heard IPs can be
collected. A PC messenger may broadcast packets without hosting an HTTP endpoint.

Collection requests the selected sender's /messages.json endpoint. This is
HTTP retrieval, not passive interception. It reads the public total and encrypted
packet and uses its own copy of the wafq function (include/wafq.h) to reconstruct the square.
It neither decrypts nor changes messages. The sender exposes its latest 10
records; collect regularly to avoid missing older messages. Total <30 has no square.

The observer keeps its latest 40 collected records in RAM until restart/power
loss. Evictions are counted. IDs/order refer to collection order, not sentence
positions or a global timeline. Senders currently expose no sentence metadata.

## Laptop terminal (Windows PowerShell)

From this repo's folder:

```powershell
.\sniff.ps1 COM11
```

At the sniff prompt:

```text
ips
collect 10.12.125.109
dump
quit
```

Use the IP actually listed by ips. The laptop talks only over USB serial; the
observer makes the HTTP request. One-shot commands are also available:

```powershell
.\sniff.ps1 COM11 dump
.\sniff.ps1 COM11 dump -Json > captures.jsonl
```

JSON output includes totals, squares, source IPs, encrypted packet hex and a
status record. Human output omits encrypted hex. Dump does not erase the board.
Close other serial monitors before using the command. Status shows the board's
current address and retention counts. Discovery continues while connected.
During a collection request the single-threaded board may miss packets; HTTP
connect/read timeouts are 1.5 seconds. Disconnecting USB also loses RAM if it
removes the board's only power source.

## Build and tests

Vendor display/touch dependencies:
https://github.com/Xinyuan-LilyGO/T-Display-S3
Revision ec889e789b3cf093412689a143f7f37b42b56af7.
ArduinoJson 6.21.5 is pinned in platformio.ini. After cloning, fetch the vendor
files into .vendor/ (not committed):

```powershell
git clone --filter=blob:none --sparse https://github.com/Xinyuan-LilyGO/T-Display-S3.git .vendor/T-Display-S3
git -C .vendor/T-Display-S3 checkout ec889e789b3cf093412689a143f7f37b42b56af7
git -C .vendor/T-Display-S3 sparse-checkout set lib/TFT_eSPI lib/SensorLib
```

Copy include/secrets.example.h to include/secrets.h and fill in the Wi-Fi.

```powershell
pio test --upload-port COM11 --test-port COM11
pio run -t upload --upload-port COM11
```

Tests temporarily flash a test runner; always restore the observer afterwards.
They cover exact reconstruction of total 256, deduplication, repeated totals,
source separation, atomic invalid-response rejection, and bounded retention.

Verified 2026-09-26 on COM11: all four on-board tests passed. The observer discovered 10.12.125.109 from a fresh broadcast,
collected totals 131, 256 and 48, and exported their squares through sniff.ps1.
USB-exported encrypted packets and totals matched the live sender endpoint.
Repeat collection added zero duplicates. Upload and PowerShell parsing passed.
