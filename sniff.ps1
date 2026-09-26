param(
    [string]$Port = 'COM11',
    [string]$Command = '',
    [switch]$Json
)
$ErrorActionPreference = 'Stop'
$serial = [System.IO.Ports.SerialPort]::new($Port, 115200)
$serial.NewLine = "`n"
$serial.ReadTimeout = 500
$serial.WriteTimeout = 2000
$serial.Encoding = [System.Text.Encoding]::UTF8

function Show-Record($record) {
    if ($Json) { $record | ConvertTo-Json -Depth 6 -Compress; return }
    switch ($record.type) {
        'peer' { '{0,-16} packets {1,-5} ABJ1 {2,-5} last seen {3}s ago' -f $record.ip, $record.packets, $record.abj1, $record.ageSeconds }
        'status' {
            'Observer {0} | listening: {1} | IPs: {2} | stored: {3} | evicted: {4}' -f $record.ip, $record.listening, $record.peers, $record.stored, $record.evicted
            if ($record.ignoredPeers) { 'Peer list full: {0} packets from additional IPs ignored' -f $record.ignoredPeers }
        }
        'collected' { 'Collected {0} new records from {1}; {2} stored, {3} evicted.' -f $record.added, $record.ip, $record.stored, $record.evicted }
        'error' { 'Device: ' + $record.message }
        'message' {
            ''
            '#{0}  FROM {1}  abjad {2}' -f $record.id, $record.source, $record.total
            if ($record.square.Count -eq 16) {
                for ($row = 0; $row -lt 4; $row++) {
                    ($record.square[($row * 4)..($row * 4 + 3)] | ForEach-Object { '{0,10}' -f $_ }) -join ''
                }
            } else { '  Below 30: no square' }
        }
    }
}
function Send-Command([string]$text) {
    if ($text -notmatch '^(ips|status|dump|collect\s+\d{1,3}(\.\d{1,3}){3})$') {
        'Commands: ips | collect <ip> | dump | status | quit'
        return
    }
    $serial.DiscardInBuffer()
    $serial.WriteLine($text)
    $deadline = (Get-Date).AddSeconds(20)
    while ((Get-Date) -lt $deadline) {
        try { $line = $serial.ReadLine().Trim() } catch [TimeoutException] { continue }
        if ($line -eq '@@END') { return }
        if ($line.StartsWith('@@{')) { Show-Record ($line.Substring(2) | ConvertFrom-Json) }
    }
    throw 'Observer did not complete the command. Check the USB port and firmware.'
}
try {
    $serial.Open()
    if ($Command) { Send-Command $Command }
    else {
        'Talsam observer: ips | collect <ip> | dump | status | quit'
        'IP discovery listens for UDP 4646 broadcasts. Collection fetches public squares.'
        Send-Command 'status'
        while ($true) {
            $text = (Read-Host 'sniff').Trim()
            if ($text -eq 'quit') { break }
            if ($text) { Send-Command $text }
        }
    }
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
