import { app, BrowserWindow, ipcMain, shell } from 'electron';
import path from 'path';
import { exec } from 'child_process';
import fs from 'fs';
import koffi from 'koffi';

declare const MAIN_WINDOW_VITE_DEV_SERVER_URL: string;
declare const MAIN_WINDOW_VITE_NAME: string;

let mainWindow: BrowserWindow | null = null;

const dllPath = app.isPackaged
  ? path.join(process.resourcesPath, 'TaskbarEffects.dll')
  : path.resolve(process.cwd(), 'TaskbarEffects.dll');

console.log('Attempting to load DLL from:', dllPath);

let ExecuteTaskbarLogic: any = null;

try {
  const lib = koffi.load(dllPath);
  ExecuteTaskbarLogic = lib.func('ExecuteTaskbarLogic', 'string', ['string']);
} catch (error) {
  console.error('CRITICAL: Failed to load TaskbarEffects.dll:', error);
}

const createWindow = (): void => {
  mainWindow = new BrowserWindow({
    width: 1800,
    height: 800,
    resizable: false,
    frame: false,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false
    },
  });

  if (MAIN_WINDOW_VITE_DEV_SERVER_URL) {
    mainWindow.loadURL(MAIN_WINDOW_VITE_DEV_SERVER_URL);
  } else {
    mainWindow.loadFile(path.join(__dirname, `../renderer/${MAIN_WINDOW_VITE_NAME}/index.html`));
  }
};

ipcMain.handle('send-taskbar-command', async (event, commandStr: string) => {
  console.log('-> IPC Text Command:', commandStr);
  if (ExecuteTaskbarLogic) {
    return ExecuteTaskbarLogic(commandStr);
  }
  return 'DLL not loaded';
});

ipcMain.on('update-taskbar-style', (event, config) => {

  if (ExecuteTaskbarLogic) {
    const payload = JSON.stringify({
      command: "UPDATE_STYLE",
      radius: Math.floor(config.radius),
      padding: Math.floor(config.padding),
      iconSize: Math.floor(config.iconSize),
      margins: Math.floor(config.margins || 44),

      bgA: Math.floor(config.bgA),
      bgR: Math.floor(config.bgR),
      bgG: Math.floor(config.bgG),
      bgB: Math.floor(config.bgB),

      borderA: Math.floor(config.borderA),
      borderR: Math.floor(config.borderR),
      borderG: Math.floor(config.borderG),
      borderB: Math.floor(config.borderB),

      isDirty: true
    });

    ExecuteTaskbarLogic(payload);
  }
});

ipcMain.on('change-taskbar-dll', (event, dllName: string) => {
  console.log('-> IPC Style DLL Change:', dllName);
  if (ExecuteTaskbarLogic) {
    ExecuteTaskbarLogic(`STYLE:${dllName}`);
  }
});

app.on('ready', createWindow);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

ipcMain.handle('get-wallpaper', async () => {
  return new Promise((resolve) => {
    const cmd = `powershell -command "(Get-ItemProperty -Path 'HKCU:\\Control Panel\\Desktop').Wallpaper"`;

    exec(cmd, (error, stdout) => {
      if (error) {
        resolve(null);
        return;
      }

      const wallpaperPath = stdout.trim();

      try {
        if (wallpaperPath && fs.existsSync(wallpaperPath)) {
          const bitmap = fs.readFileSync(wallpaperPath);
          const base64 = Buffer.from(bitmap).toString('base64');
          resolve(`data:image/jpeg;base64,${base64}`);
        } else {
          resolve(null);
        }
      } catch (e) {
        resolve(null);
      }
    });
  });
});

ipcMain.handle('open-external', async (event, url) => {
  try {
    await shell.openExternal(url);
  } catch (error) {
    console.error("Failed to open URL:", error);
  }
});