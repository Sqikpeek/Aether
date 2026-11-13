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
export declare class ServerPinger {
    /**
     * Pings a Minecraft server using the standard Server List Ping (SLP) protocol.
     */
    static ping(host: string, port?: number, timeoutMs?: number): Promise<ServerPingResult>;
    private static encodeVarInt;
    private static decodeVarInt;
}
