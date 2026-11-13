import fs from 'node:fs';
let cachedCa = null;
let caChecked = false;
export function getSystemCa() {
    if (caChecked) {
        return cachedCa || undefined;
    }
    caChecked = true;
    const commonPaths = [
        process.env.NODE_EXTRA_CA_CERTS,
        '/etc/ssl/certs/ca-certificates.crt',
        '/etc/pki/tls/certs/ca-bundle.crt',
        '/etc/ssl/ca-bundle.pem'
    ].filter(Boolean);
    for (const p of commonPaths) {
        if (fs.existsSync(p)) {
            try {
                cachedCa = fs.readFileSync(p);
                return cachedCa;
            }
            catch { }
        }
    }
    return undefined;
}
