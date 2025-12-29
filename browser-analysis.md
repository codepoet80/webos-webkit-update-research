# WebKit Browser Source Analysis

## Source Packages Found

| Package | Contents |
|---------|----------|
| `webkit.tgz` + patch | WebKit shell, platform ports (Qt, GTK, Chromium, **Palm**) |
| `webcore.tgz` + patch | Core rendering engine with **Palm platform layer** |
| `javascriptcore.tgz` + patch | JavaScript engine |
| `qt4-4.8.0.tar.gz` + patch | Qt 4.8 with Palm modifications |

## WebKit Version

Based on ChangeLog: **~October 2009** (WebKit trunk)
- Pre-WebKit2 split
- V8 as JavaScript engine option
- Basic HTML5 support

## Palm Platform Port Structure

### WebKit/palm/ (API Layer)
```
Api/
├── palmwebview.cpp/h         # Main browser view
├── palmwebpage.cpp/h         # Page management
├── palmwebframe.cpp/h        # Frame handling
├── palmwebtypes.h            # Type definitions
├── palmerrorcodes.cpp/h      # Error mapping
├── palmfpshandler.cpp/h      # FPS monitoring
├── palmmemstats.cpp/h        # Memory statistics
├── palmwebsslinfo.h          # SSL certificate info
└── webkitstats.cpp/h         # Performance stats

WebCoreSupport/
├── ChromeClientPalm.cpp/h    # Browser chrome (dialogs, alerts)
├── ContextMenuClientPalm.cpp/h
├── DragClientPalm.cpp/h
├── EditorClientPalm.cpp/h    # Text editing
├── FrameLoaderClientPalm.cpp/h # Page loading
├── InspectorClientPalm.cpp/h # Dev tools
├── GLES2Context*.cpp/h       # GPU acceleration
├── WebGraphicsContext3D*.cpp/h
├── SpellCheck.cpp/h
├── SmartTextEngine.cpp/h
└── WordCompletionController.cpp/h

webkit/
├── webkitpalmclipboard.cpp/h
├── webkitpalmsettings.cpp/h
├── webkitpalmstrings.cpp/h
├── webkitpalmtimer.cpp/h
├── webkitpalmwindow.h
└── weboseventreporter_webkit.cpp/h
```

### WebCore/platform/palm/ (Platform Layer)

**Critical Luna Integration:**
```
LunaServiceMgr.cpp/h          # Luna Service Bus connection
PalmServiceBridge.cpp/h/idl   # JavaScript <-> Luna bridge
V8PalmServiceBridgeCustom.cpp # V8 bindings for services
```

**Graphics Layer:**
```
graphics/palm/
├── GraphicsLayerPalm.cpp/h   # Compositing layer
├── LayerRendererPalm.cpp/h   # GPU-accelerated rendering
├── LayerPalm.cpp/h           # Base layer class
├── ContentLayerPalm.cpp/h    # Content tiles
├── CanvasLayerPalm.cpp/h     # HTML5 Canvas
├── ImageLayerPalm.cpp/h      # Image layers
├── VideoLayerPalm.cpp/h      # Video playback
├── MediaPlayerPrivatePalm.cpp/h
└── BasicLayerPalm.cpp/h
```

**Network Layer:**
```
network/palm/
├── ResourceHandlePalm.cpp    # HTTP requests (via libcurl)
├── CurlHandle.cpp/h          # CURL wrapper
├── CurlHandlePool.cpp/h      # Connection pooling
├── AsyncLoader.cpp/h         # Async loading
├── DirectLoader.cpp/h        # Sync loading
├── DiskCachePalm.cpp/h       # Disk cache
├── CredentialsCache.cpp/h    # Auth credentials
├── CookieJarPalm.cpp         # Cookie storage
├── SocketStreamHandlePalm.cpp # WebSocket support
└── DNSPalm.cpp               # DNS resolution
```

**Platform Services:**
```
platform/palm/
├── GeolocationServicePalm.cpp/h  # GPS
├── DeviceMotionProviderPalm.cpp/h # Accelerometer
├── DeviceOrientationProviderPalm.cpp/h
├── Sensor.cpp/h/idl          # Generic sensor API
├── SensorManager.cpp/h/idl
├── SensorObjects.cpp/h
```

**UI Components:**
```
├── RenderThemePalm.cpp/h     # Form controls styling
├── ScrollbarThemePalm.cpp/h  # Scrollbar appearance
├── PopupMenuPalm.cpp/h       # Dropdowns
├── ClipboardPalm.cpp/h       # Copy/paste
├── ClipboardWidget*.cpp/h    # Selection UI
├── SpellingWidget*.cpp/h     # Spell check UI
├── SelectionMarkersWidget*.cpp/h # Selection handles
```

## Key APIs to Preserve

### 1. PalmServiceBridge (Critical)
This is how web apps communicate with system services:
```javascript
// Example usage in webOS apps
var bridge = new PalmServiceBridge();
bridge.call("palm://com.palm.systemservice/getPreferences",
            '{"keys":["locale"]}',
            handleResponse);
```

### 2. webOS-specific CSS
```css
/* Custom properties */
-webos-tap-highlight-color
-webos-system-font-family
```

### 3. JavaScript APIs
- `PalmSystem` object
- `PalmServiceBridge` class
- Sensor/SensorManager APIs
- Mojo/Enyo framework hooks

## Build System

The original build used:
- OpenEmbedded/BitBake
- qmake for Qt integration
- Custom GYP/GYI files for chromium-style build

## Dependencies

From package control files:
- Qt 4.8.0 (modified)
- V8 2.5.9.22
- libcurl 7.21.7
- ICU 3.6
- libxml2 2.7.2
- libxslt 1.1.17
- freetype 2.3.12
- glib 2.16.6
- SQLite 3.6.20
- Piranha (Palm graphics library) 1.2
- liblunaservice 2.0.0
- pbnjson 1.0.1
- eventreporter 1.1

## Upgrade Path Options

### Option A: Rebase to WebKit ~2012-2013
- Last version before major WebKit2 split
- Still compatible with Qt 4.x
- Would need to port all Palm/* files forward
- Estimated effort: Very High (weeks of work)

### Option B: Rebase to QtWebKit 2.3
- Last standalone QtWebKit release
- Better compatibility with Qt 4.8
- Still requires porting Palm integration
- Estimated effort: High

### Option C: Security Patches Only
- Keep existing WebKit base
- Backport security fixes
- Update SSL/TLS support (tied to OpenSSL update)
- Estimated effort: Medium

### Option D: Standalone Modern Browser
- Build separate QtWebEngine app
- Would require Qt 5.x (not available)
- Alternative: Build with WebKitGTK
- Wouldn't integrate with system
- Estimated effort: Medium-High

## Recommendation

Given the complexity, I recommend a **phased approach**:

1. **Phase 1**: Apply OpenSSL update (done!)
   - Enables TLS 1.2 for libcurl
   - Browser's HTTPS will work better

2. **Phase 2**: Security patches to existing WebKit (IN PROGRESS)
   - Focus on memory safety issues
   - XSS protection improvements
   - See `browser-src/security-patches/` directory

3. **Phase 3**: Consider QtWebKit 2.3 rebase
   - Only if Phase 2 proves insufficient
   - Would be a major undertaking

## Security Patches Created

Located in `browser-src/security-patches/`:

| Patch | CVEs | Description |
|-------|------|-------------|
| 001-rendering-null-checks.patch | CVE-2010-1813 | Null pointer fixes in rendering |
| 002-svg-uaf-protection.patch | CVE-2011-0222, CVE-2011-0240 | SVG use-after-free fixes |
| 003-css-memory-safety.patch | CVE-2010-0046 | CSS parsing memory safety |
| 004-xss-dom-hardening.patch | CVE-2009-1684, CVE-2009-1695 | XSS protections |
| 005-rtl-text-uaf.patch | CVE-2010-0049 | RTL text use-after-free |

To apply:
```bash
cd browser-src
./security-patches/apply-patches.sh
```

See `browser-src/security-patches/README.md` for full details.

## Files Summary

Total Palm-specific files:
- WebKit/palm/: ~70 files
- WebCore/platform/palm/: ~150+ files
- WebCore/platform/graphics/palm/: ~20 files
- WebCore/platform/network/palm/: ~25 files

Patch sizes:
- webkit-patch: 27,745 lines
- webcore-patch: 74,873 lines
