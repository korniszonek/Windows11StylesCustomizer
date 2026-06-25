import React, { useState, useEffect } from 'react';
import { Code } from 'lucide-react';
import './index.css';

interface Color { r: number; g: number; b: number; a: number; }
interface Config { radius: number; padding: number; iconSize: number; bgColor: Color; borderColor: Color; }

const PRESETS: Record<string, { radius: number; padding: number; iconSize: number; bgColor: Color; borderColor?: Color }> = {
    PureMinimal: {
        radius: 8,
        padding: 14,
        iconSize: 32,
        bgColor: { r: 28, g: 28, b: 28, a: 195 },
        borderColor: { r: 255, g: 255, b: 255, a: 45 }
    },
    PinkCrystal: {
        radius: 24,
        padding: 18,
        iconSize: 32,
        bgColor: { r: 255, g: 182, b: 193, a: 140 },
        borderColor: { r: 255, g: 255, b: 255, a: 110 }
    },
    CyberChroma: {
        radius: 12,
        padding: 16,
        iconSize: 32,
        bgColor: { r: 13, g: 16, b: 25, a: 220 }
    }
};

export default function App() {
    const [config, setConfig] = useState<Config>({
        radius: 12, padding: 16, iconSize: 32,
        bgColor: { r: 13, g: 16, b: 25, a: 220 },
        borderColor: { r: 0, g: 240, b: 255, a: 255 }
    });
    const [activeStyle, setActiveStyle] = useState<string>('UserPreset');
    const [wallpaper, setWallpaper] = useState<string | null>(null);

    useEffect(() => {
        if ((window as any).electronAPI && (window as any).electronAPI.getWallpaper) {
            (window as any).electronAPI.getWallpaper().then((bgData: string | null) => {
                if (bgData) setWallpaper(bgData);
            });
        }
    }, []);

    useEffect(() => {
        if (activeStyle === 'UserPreset' && (window as any).electronAPI) {
            (window as any).electronAPI.updateStyle(config);
        }
    }, [config, activeStyle]);

    const handleStyleChange = (styleName: string) => {
        setActiveStyle(styleName);
        if ((window as any).electronAPI) {
            if (styleName === 'UserPreset') {
                (window as any).electronAPI.changeStyleDll('UserPreset');
                (window as any).electronAPI.updateStyle(config);
            } else {
                (window as any).electronAPI.changeStyleDll(styleName);
            }
        }
    };

    const updateColor = (type: 'bgColor' | 'borderColor', channel: keyof Color, value: string) => {
        setConfig(prev => ({ ...prev, [type]: { ...prev[type], [channel]: parseInt(value) } }));
    };

    const handleCloseApp = () => {
        window.close();
    };

    const toRgbaString = (color: Color) => {
        return `rgba(${color.r}, ${color.g}, ${color.b}, ${color.a / 255})`;
    };

    const currentVisuals = activeStyle === 'UserPreset' ? config : PRESETS[activeStyle];
    const isCyber = activeStyle === 'CyberChroma';
    const isPink = activeStyle === 'PinkCrystal';

    const getDynamicStyles = () => {
        if (!currentVisuals) return {};

        let bgImage = 'none';
        let bgColor = toRgbaString(currentVisuals.bgColor);
        let border = currentVisuals.borderColor ? `1px solid ${toRgbaString(currentVisuals.borderColor)}` : 'none';

        let iconBg = 'rgba(255, 255, 255, 0.25)';
        let iconBorder = 'none';

        if (isPink) {
            bgColor = 'transparent';
            bgImage = `linear-gradient(to bottom, rgba(255, 240, 245, 0.7) 0%, rgba(255, 182, 193, 0.55) 100%)`;
            iconBg = 'rgba(230, 255, 255, 0.75)';
            iconBorder = '1px solid rgba(255, 255, 255, 0.4)';
        } else if (isCyber) {
            bgColor = 'transparent';
            bgImage = `linear-gradient(${toRgbaString(PRESETS.CyberChroma.bgColor)}, ${toRgbaString(PRESETS.CyberChroma.bgColor)}), linear-gradient(135deg, rgba(0, 240, 255, 1) 0%, rgba(255, 0, 128, 1) 100%)`;
            border = '1.5px solid transparent';
            iconBg = 'rgba(0, 240, 255, 0.35)';
            iconBorder = '1px solid rgba(0, 240, 255, 0.7)';
        }

        return {
            '--tb-radius': `${currentVisuals.radius / 2}px`,
            '--tb-padding': `${currentVisuals.padding / 3}px ${currentVisuals.padding / 1.5}px`,
            '--tb-bg-color': bgColor,
            '--tb-bg-image': bgImage,
            '--tb-border': border,
            '--tb-bg-origin': isCyber ? 'border-box' : 'padding-box',
            '--tb-bg-clip': isCyber ? 'content-box, border-box' : 'padding-box',
            '--icon-size': `${currentVisuals.iconSize / 1.5}px`,
            '--icon-margin': `0 ${currentVisuals.padding / 4}px`,
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
                        <input type="range" min="0" max="40" value={config.radius} onChange={e => setConfig(prev => ({ ...prev, radius: parseInt(e.target.value) }))} />
                    </div>
                    <div className="slider-row">
                        <span>Icon Padding ({config.padding}px)</span>
                        <input type="range" min="4" max="32" value={config.padding} onChange={e => setConfig(prev => ({ ...prev, padding: parseInt(e.target.value) }))} />
                    </div>
                    <div className="slider-row">
                        <span>Icon Size ({config.iconSize}px)</span>
                        <input type="range" min="16" max="48" value={config.iconSize} onChange={e => setConfig(prev => ({ ...prev, iconSize: parseInt(e.target.value) }))} />
                    </div>

                    <div className="section-title">Background Color (RGBA)</div>
                    <div className="color-row">
                        <input type="range" min="0" max="255" value={config.bgColor.r} onChange={e => updateColor('bgColor', 'r', e.target.value)} />
                        <input type="range" min="0" max="255" value={config.bgColor.g} onChange={e => updateColor('bgColor', 'g', e.target.value)} />
                        <input type="range" min="0" max="255" value={config.bgColor.b} onChange={e => updateColor('bgColor', 'b', e.target.value)} />
                        <input type="range" min="0" max="255" value={config.bgColor.a} onChange={e => updateColor('bgColor', 'a', e.target.value)} />
                    </div>

                    <div className="section-title">Border Color (RGBA)</div>
                    <div className="color-row">
                        <input type="range" min="0" max="255" value={config.borderColor.r} onChange={e => updateColor('borderColor', 'r', e.target.value)} />
                        <input type="range" min="0" max="255" value={config.borderColor.g} onChange={e => updateColor('borderColor', 'g', e.target.value)} />
                        <input type="range" min="0" max="255" value={config.borderColor.b} onChange={e => updateColor('borderColor', 'b', e.target.value)} />
                        <input type="range" min="0" max="255" value={config.borderColor.a} onChange={e => updateColor('borderColor', 'a', e.target.value)} />
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
                            if ((window as any).electronAPI && (window as any).electronAPI.openExternal) {
                                (window as any).electronAPI.openExternal('https://github.com/korniszonek');
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