import { app, BrowserWindow, ipcMain ,shell} from 'electron';
import path from 'path';
import net from 'net';
import { exec } from 'child_process';
import fs from 'fs';

declare const MAIN_WINDOW_VITE_DEV_SERVER_URL: string;
declare const MAIN_WINDOW_VITE_NAME: string;

let mainWindow: BrowserWindow | null = null;
const PIPE_PATH = '\\\\.\\pipe\\WindowsTaskbarConfigPipe';

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
  // mainWindow.webContents.openDevTools();

  if (MAIN_WINDOW_VITE_DEV_SERVER_URL) {
    mainWindow.loadURL(MAIN_WINDOW_VITE_DEV_SERVER_URL);
  } else {
    mainWindow.loadFile(path.join(__dirname, `../renderer/${MAIN_WINDOW_VITE_NAME}/index.html`));
  }
};

function sendToPipe(data: string | Buffer): void {
  const client = net.createConnection(PIPE_PATH, () => {
    client.write(data);
    client.end();
  });

  client.on('error', (err) => {
    console.log("Pipe connection error:", err.message);
  });
}

ipcMain.on('update-taskbar-style', (event, config) => {
  const payload = JSON.stringify({
    command: "UPDATE_STYLE",
    radius: Math.floor(config.radius),
    padding: Math.floor(config.padding),
    iconSize: Math.floor(config.iconSize),
    bg_color: {
      a: Math.floor(config.bgColor.a),
      r: Math.floor(config.bgColor.r),
      g: Math.floor(config.bgColor.g),
      b: Math.floor(config.bgColor.b)
    },
    border_color: {
      a: Math.floor(config.borderColor.a),
      r: Math.floor(config.borderColor.r),
      g: Math.floor(config.borderColor.g),
      b: Math.floor(config.borderColor.b)
    }
  });

  sendToPipe(payload);
});

ipcMain.on('change-taskbar-dll', (event, dllName: string) => {
  sendToPipe(`STYLE:${dllName}`);
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