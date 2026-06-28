import React, { useState, useEffect } from 'react';
import { Code } from 'lucide-react';
import './index.css';

interface Config {
    radius: number;
    padding: number;
    iconSize: number;
    margins: number;
    bgA: number;
    bgR: number;
    bgG: number;
    bgB: number;
    borderA: number;
    borderR: number;
    borderG: number;
    borderB: number;
    isDirty: boolean;
}

declare global {
    interface Window {
        electronAPI?: {
            getWallpaper(): Promise<string | null>;
            openExternal(url: string): Promise<void>;
            sendCommandToTaskbar(commandString: string): Promise<string>;
            updateStyle(config: Config): void;
            changeStyleDll(styleName: string): void;
        };
    }
}

const PRESETS: Record<string, Omit<Config, 'isDirty'>> = {
    PureMinimal: {
        radius: 8,
        padding: 14,
        iconSize: 32,
        margins: 44,
        bgR: 28, bgG: 28, bgB: 28, bgA: 195,
        borderR: 255, borderG: 255, borderB: 255, borderA: 45
    },
    PinkCrystal: {
        radius: 24,
        padding: 18,
        iconSize: 32,
        margins: 44,
        bgR: 255, bgG: 182, bgB: 193, bgA: 140,
        borderR: 255, borderG: 255, borderB: 255, borderA: 110
    },
    CyberChroma: {
        radius: 12,
        padding: 16,
        iconSize: 32,
        margins: 44,
        bgR: 13, bgG: 16, bgB: 25, bgA: 220,
        borderR: 0, borderG: 240, borderB: 255, borderA: 255
    }
};

export default function App() {
    const [config, setConfig] = useState<Config>({
        radius: 12, padding: 16, iconSize: 32, margins: 44,
        bgR: 13, bgG: 16, bgB: 25, bgA: 220,
        borderR: 0, borderG: 240, borderB: 255, borderA: 255,
        isDirty: true
    });
    const [activeStyle, setActiveStyle] = useState<string>('UserPreset');
    const [wallpaper, setWallpaper] = useState<string | null>(null);

    useEffect(() => {
        if (window.electronAPI?.getWallpaper) {
            window.electronAPI.getWallpaper().then((bgData: string | null) => {
                if (bgData) setWallpaper(bgData);
            });
        }
    }, []);

    useEffect(() => {
        if (activeStyle === 'UserPreset' && window.electronAPI?.updateStyle) {
            window.electronAPI.updateStyle(config);
        }
    }, [config, activeStyle]);

    const sendCommandToTaskbar = async (commandString: string): Promise<void> => {
        try {
            if (window.electronAPI?.sendCommandToTaskbar) {
                const response = await window.electronAPI.sendCommandToTaskbar(commandString);
                console.log('DLL Response:', response);
            }
        } catch (error) {
            console.error('Failed to communicate with main process:', error);
        }
    };

    const handleStyleChange = (styleName: string) => {
        setActiveStyle(styleName);

        if (!window.electronAPI) return;

        if (styleName === 'UserPreset') {
            window.electronAPI.changeStyleDll('UserPreset');
            window.electronAPI.updateStyle({ ...config, isDirty: true });
        } else {
            window.electronAPI.changeStyleDll(styleName);

            const presetConfig = PRESETS[styleName];
            if (presetConfig) {
                window.electronAPI.updateStyle({
                    ...presetConfig,
                    isDirty: true 
                });
            }
        }
    };

    const updateColorKey = (key: keyof Config, value: string) => {
        setConfig(prev => ({ ...prev, [key]: parseInt(value), isDirty: true }));
    };

    const handleCloseApp = () => {
        window.close();
    };

    const currentVisuals = activeStyle === 'UserPreset' ? config : PRESETS[activeStyle];
    const isCyber = activeStyle === 'CyberChroma';
    const isPink = activeStyle === 'PinkCrystal';

    const getDynamicStyles = () => {
        if (!currentVisuals) return {};

        const cv = currentVisuals;
        let bgImage = 'none';
        let bgColor = `rgba(${cv.bgR}, ${cv.bgG}, ${cv.bgB}, ${cv.bgA / 255})`;
        let border = `1px solid rgba(${cv.borderR}, ${cv.borderG}, ${cv.borderB}, ${cv.borderA / 255})`;

        let iconBg = 'rgba(255, 255, 255, 0.25)';
        let iconBorder = 'none';

        if (isPink) {
            bgColor = 'transparent';
            bgImage = `linear-gradient(to bottom, rgba(255, 240, 245, 0.7) 0%, rgba(255, 182, 193, 0.55) 100%)`;
            iconBg = 'rgba(230, 255, 255, 0.75)';
            iconBorder = '1px solid rgba(255, 255, 255, 0.4)';
        } else if (isCyber) {
            bgColor = 'transparent';
            bgImage = `linear-gradient(rgba(${PRESETS.CyberChroma.bgR}, ${PRESETS.CyberChroma.bgG}, ${PRESETS.CyberChroma.bgB}, ${PRESETS.CyberChroma.bgA / 255}), rgba(${PRESETS.CyberChroma.bgR}, ${PRESETS.CyberChroma.bgG}, ${PRESETS.CyberChroma.bgB}, ${PRESETS.CyberChroma.bgA / 255})), linear-gradient(135deg, rgba(0, 240, 255, 1) 0%, rgba(255, 0, 128, 1) 100%)`;
            border = '1.5px solid transparent';
            iconBg = 'rgba(0, 240, 255, 0.35)';
            iconBorder = '1px solid rgba(0, 240, 255, 0.7)';
        }

        return {
            '--tb-radius': `${cv.radius / 2}px`,
            '--tb-padding': `${cv.padding / 3}px ${cv.padding / 1.5}px`,
            '--tb-bg-color': bgColor,
            '--tb-bg-image': bgImage,
            '--tb-border': border,
            '--tb-bg-origin': isCyber ? 'border-box' : 'padding-box',
            '--tb-bg-clip': isCyber ? 'content-box, border-box' : 'padding-box',
            '--icon-size': `${cv.iconSize / 1.5}px`,
            '--icon-margin': `0 ${cv.padding / 4}px`,
            '--icon-bg': iconBg,
            '--icon-border': iconBorder,
        } as React.CSSProperties;
    };

    return (
        <div className="window-container" style={getDynamicStyles()}>
            <div className="titlebar-drag"></div>

            <button onClick={handleCloseApp} className="close-btn">✕</button>

            <div className="left-card">
                <h2 className="title">Personalize Taskbar</h2>
                <p className="subtitle">Choose a preset theme or build your own layout.</p>

                <div className="btn-group">
                    <div className={`style-btn ${activeStyle === 'PureMinimal' ? 'active' : ''}`} onClick={() => handleStyleChange('PureMinimal')}>
                        <span>Pure Minimal Preset</span>
                        <span className="action-badge">Apply</span>
                    </div>
                    <div className={`style-btn ${activeStyle === 'PinkCrystal' ? 'active' : ''}`} onClick={() => handleStyleChange('PinkCrystal')}>
                        <span>Pink Crystal Preset</span>
                        <span className="action-badge">Apply</span>
                    </div>
                    <div className={`style-btn ${activeStyle === 'CyberChroma' ? 'active' : ''}`} onClick={() => handleStyleChange('CyberChroma')}>
                        <span>Cyber Chroma Preset</span>
                        <span className="action-badge">Apply</span>
                    </div>
                    <div className={`style-btn ${activeStyle === 'UserPreset' ? 'active' : ''}`} onClick={() => handleStyleChange('UserPreset')}>
                        <span>Custom User Preset</span>
                        <span className="action-badge">Active</span>
                    </div>
                </div>

                <div className={`slider-container ${activeStyle !== 'UserPreset' ? 'disabled' : ''}`}>
                    <div className="section-title">Geometry Settings</div>
                    <div className="slider-row">
                        <span>Corner Radius ({config.radius}px)</span>
                        <input type="range" min="0" max="40" value={config.radius} onChange={e => setConfig(prev => ({ ...prev, radius: parseInt(e.target.value), isDirty: true }))} />
                    </div>
                    <div className="slider-row">
                        <span>Icon Padding ({config.padding}px)</span>
                        <input type="range" min="4" max="32" value={config.padding} onChange={e => setConfig(prev => ({ ...prev, padding: parseInt(e.target.value), isDirty: true }))} />
                    </div>
                    <div className="slider-row">
                        <span>Icon Size ({config.iconSize}px)</span>
                        <input type="range" min="16" max="48" value={config.iconSize} onChange={e => setConfig(prev => ({ ...prev, iconSize: parseInt(e.target.value), isDirty: true }))} />
                    </div>

                    <div className="section-title">Background Color (RGBA)</div>
                    <div className="color-row">
                        <input type="range" min="0" max="255" value={config.bgR} onChange={e => updateColorKey('bgR', e.target.value)} />
                        <input type="range" min="0" max="255" value={config.bgG} onChange={e => updateColorKey('bgG', e.target.value)} />
                        <input type="range" min="0" max="255" value={config.bgB} onChange={e => updateColorKey('bgB', e.target.value)} />
                        <input type="range" min="0" max="255" value={config.bgA} onChange={e => updateColorKey('bgA', e.target.value)} />
                    </div>

                    <div className="section-title">Border Color (RGBA)</div>
                    <div className="color-row">
                        <input type="range" min="0" max="255" value={config.borderR} onChange={e => updateColorKey('borderR', e.target.value)} />
                        <input type="range" min="0" max="255" value={config.borderG} onChange={e => updateColorKey('borderG', e.target.value)} />
                        <input type="range" min="0" max="255" value={config.borderB} onChange={e => updateColorKey('borderB', e.target.value)} />
                        <input type="range" min="0" max="255" value={config.borderA} onChange={e => updateColorKey('borderA', e.target.value)} />
                    </div>

                </div>
            </div>

            <div className="right-panel">
                <div
                    className="preview-screen"
                    style={{ backgroundImage: wallpaper ? `url(${wallpaper})` : 'none' }}
                >
                    <div className="preview-watermark">Live Preview</div>

                    <div className="taskbar-preview">
                        <div className="preview-icons-group">
                            <div className="preview-icon"></div>
                            <div className="preview-icon"></div>
                            <div className="preview-icon"></div>
                            <div className="preview-icon"></div>
                        </div>
                    </div>
                </div>

                <div className="github-footer">
                    <a
                        href="#"
                        onClick={(e) => {
                            e.preventDefault();
                            if (window.electronAPI?.openExternal) {
                                window.electronAPI.openExternal('https://github.com/korniszonek');
                            }
                        }}
                        className="github-link"
                    >
                        <Code size={16} />
                        <span>korniszonek</span>
                    </a>
                </div>
            </div>
        </div>
    );
}