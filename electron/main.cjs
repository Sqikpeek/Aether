const { app, BrowserWindow, shell, ipcMain } = require('electron');
const path = require('node:path');
const { LauncherServer } = require('../dist/server/api.js');

let mainWindow = null;
let server = null;

async function startApp() {
  server = new LauncherServer(3000);
  const port = await server.start();

  mainWindow = new BrowserWindow({
    width: 1150,
    height: 730,
    minWidth: 920,
    minHeight: 620,
    title: 'Aether Launcher',
    backgroundColor: '#0c0d12',
    autoHideMenuBar: true,
    show: false,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true
    }
  });

  mainWindow.loadURL(`http://localhost:${port}`);

  mainWindow.once('ready-to-show', () => {
    mainWindow.show();
  });

  mainWindow.webContents.setWindowOpenHandler(({ url }) => {
    shell.openExternal(url);
    return { action: 'deny' };
  });

  mainWindow.on('closed', async () => {
    mainWindow = null;
    if (server) {
      await server.stop();
    }
    app.quit();
  });
}

app.whenReady().then(startApp);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
