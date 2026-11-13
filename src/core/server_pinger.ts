import net from 'node:net';

export interface ServerPingResult {
  host: string;
  port: number;
  online: boolean;
  latencyMs: number;
  versionName?: string;
  protocolVersion?: number;
  playersOnline?: number;
  playersMax?: number;
  motdText?: string;
  favicon?: string;
  error?: string;
}

export class ServerPinger {
  /**
   * Pings a Minecraft server using the standard Server List Ping (SLP) protocol.
   */
  public static async ping(host: string, port = 25565, timeoutMs = 5000): Promise<ServerPingResult> {
    const startTime = Date.now();

    return new Promise(resolve => {
      const socket = new net.Socket();
      let resolved = false;

      const finish = (result: Partial<ServerPingResult>) => {
        if (resolved) return;
        resolved = true;
        socket.destroy();
        resolve({
          host,
          port,
          online: false,
          latencyMs: Date.now() - startTime,
          ...result
        });
      };

      socket.setTimeout(timeoutMs);

      socket.on('timeout', () => {
        finish({ error: 'Connection timed out' });
      });

      socket.on('error', err => {
        finish({ error: err.message });
      });

      socket.connect(port, host, () => {
        const latencyMs = Date.now() - startTime;

        try {
          // Packet 1: Handshake (0x00)
          // Fields: Protocol version (VarInt -1 = 47), Host string, Port (unsigned short), Next state (VarInt 1 for status)
          const hostBuf = Buffer.from(host, 'utf-8');
          const handshakePayload = Buffer.concat([
            Buffer.from([0x00]), // Packet ID 0
            ServerPinger.encodeVarInt(47), // Protocol version
            ServerPinger.encodeVarInt(hostBuf.length),
            hostBuf,
            Buffer.from([(port >> 8) & 0xff, port & 0xff]),
            ServerPinger.encodeVarInt(1) // Next state: 1 (status)
          ]);

          const handshakePacket = Buffer.concat([
            ServerPinger.encodeVarInt(handshakePayload.length),
            handshakePayload
          ]);

          // Packet 2: Status Request (0x00, empty payload)
          const requestPayload = Buffer.from([0x00]);
          const requestPacket = Buffer.concat([
            ServerPinger.encodeVarInt(requestPayload.length),
            requestPayload
          ]);

          socket.write(Buffer.concat([handshakePacket, requestPacket]));
        } catch (err: any) {
          finish({ error: `Packet creation failed: ${err.message}` });
        }
      });

      let receivedData = Buffer.alloc(0);

      socket.on('data', (chunk: Buffer | string) => {
        const buf = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
        receivedData = Buffer.concat([receivedData, buf]);

        try {
          // Try parsing response
          // Response format: Packet length (VarInt), Packet ID (VarInt 0x00), JSON String Length (VarInt), JSON String (UTF-8)
          let offset = 0;
          const { value: packetLength, bytesRead: lenBytes } = ServerPinger.decodeVarInt(receivedData, offset);
          offset += lenBytes;

          if (receivedData.length < packetLength + lenBytes) {
            // Need more data
            return;
          }

          const { value: packetId, bytesRead: idBytes } = ServerPinger.decodeVarInt(receivedData, offset);
          offset += idBytes;

          if (packetId !== 0) {
            return finish({ error: `Unexpected packet ID: ${packetId}` });
          }

          const { value: jsonLength, bytesRead: strLenBytes } = ServerPinger.decodeVarInt(receivedData, offset);
          offset += strLenBytes;

          const jsonStr = receivedData.subarray(offset, offset + jsonLength).toString('utf-8');
          const parsed = JSON.parse(jsonStr);

          let motd = '';
          if (typeof parsed.description === 'string') {
            motd = parsed.description;
          } else if (parsed.description && parsed.description.text) {
            motd = parsed.description.text;
            if (Array.isArray(parsed.description.extra)) {
              motd += parsed.description.extra.map((e: any) => (typeof e === 'string' ? e : e.text || '')).join('');
            }
          }

          // Strip Minecraft color codes (§0-§f, §k-§r) for clean display
          const cleanMotd = motd.replace(/§[0-9a-fk-or]/gi, '').trim();

          finish({
            online: true,
            latencyMs: Date.now() - startTime,
            versionName: parsed.version?.name || 'Unknown',
            protocolVersion: parsed.version?.protocol,
            playersOnline: parsed.players?.online ?? 0,
            playersMax: parsed.players?.max ?? 0,
            motdText: cleanMotd || motd || 'Minecraft Server',
            favicon: parsed.favicon
          });
        } catch {
          // Incomplete packet, wait for more data
        }
      });
    });
  }

  private static encodeVarInt(val: number): Buffer {
    const bytes: number[] = [];
    let value = val;
    while (true) {
      if ((value & ~0x7f) === 0) {
        bytes.push(value);
        return Buffer.from(bytes);
      }
      bytes.push((value & 0x7f) | 0x80);
      value >>>= 7;
    }
  }

  private static decodeVarInt(buffer: Buffer, offset = 0): { value: number; bytesRead: number } {
    let result = 0;
    let bytesRead = 0;
    let b: number;

    do {
      if (offset + bytesRead >= buffer.length) {
        throw new Error('Buffer underflow reading VarInt');
      }
      b = buffer[offset + bytesRead];
      result |= (b & 0x7f) << (7 * bytesRead);
      bytesRead++;
      if (bytesRead > 5) {
        throw new Error('VarInt is too large');
      }
    } while ((b & 0x80) !== 0);

    return { value: result, bytesRead };
  }
}
