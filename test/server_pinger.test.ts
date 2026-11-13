import test from 'node:test';
import assert from 'node:assert';
import { ServerPinger } from '../dist/core/server_pinger.js';

test('ServerPinger VarInt encoding and decoding roundtrip', () => {
  // Access private static methods via class indexing
  const encodeVarInt = (ServerPinger as any).encodeVarInt;
  const decodeVarInt = (ServerPinger as any).decodeVarInt;

  const testValues = [0, 1, 127, 128, 255, 25565, 2097151];
  for (const val of testValues) {
    const encoded: Buffer = encodeVarInt(val);
    const { value, bytesRead } = decodeVarInt(encoded, 0);
    assert.strictEqual(value, val);
    assert.strictEqual(bytesRead, encoded.length);
  }
});
