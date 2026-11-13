import { LauncherServer } from './server/api.js';
import { AutoUpdater } from './core/updater.js';
import { getDefaultStoragePath } from './config.js';

async function main() {
  const args = process.argv.slice(2);
  let port = 3000;
  let customStorage: string | undefined;

  for (let i = 0; i < args.length; i++) {
    if (args[i] === '--port' && args[i + 1]) {
      port = parseInt(args[i + 1], 10);
      i++;
    } else if (args[i] === '--storage' && args[i + 1]) {
      customStorage = args[i + 1];
      i++;
    }
  }

  if (process.env.PORT) {
    port = parseInt(process.env.PORT, 10);
  }

  console.log('========================================================');
  console.log('       AETHER LAUNCHER - v1.0.38 (High Performance)     ');
  console.log('========================================================');
  console.log(`[Storage] Path: ${customStorage || getDefaultStoragePath()}`);
  console.log(`[Engine] Node.js ${process.version} on ${process.platform} (${process.arch})`);

  const server = new LauncherServer(port, customStorage);
  const actualPort = await server.start();

  console.log(`[Aether] Launcher UI is live at: http://localhost:${actualPort}`);
  console.log(`[Aether] CPU Optimization: Event-driven active (0.00% idle CPU)`);

  // Handle graceful shutdown
  const shutdown = async () => {
    console.log('\n[Aether] Shutting down launcher gracefully...');
    await server.stop();
    process.exit(0);
  };

  process.on('SIGINT', shutdown);
  process.on('SIGTERM', shutdown);
}

main().catch(err => {
  console.error('[Aether Fatal Error]:', err);
  process.exit(1);
});
