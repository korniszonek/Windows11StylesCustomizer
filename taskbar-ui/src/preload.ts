import { contextBridge, ipcRenderer } from 'electron';

contextBridge.exposeInMainWorld('electronAPI', {
    updateStyle: (config: any) => ipcRenderer.send('update-taskbar-style', config),
    changeStyleDll: (name: string) => ipcRenderer.send('change-taskbar-dll', name),
    getWallpaper: () => ipcRenderer.invoke('get-wallpaper'),
    openExternal: (url: string) => ipcRenderer.invoke('open-external', url)
});