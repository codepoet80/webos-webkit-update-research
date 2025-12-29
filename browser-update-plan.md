# WebOS Browser Update Plan

## Overview

This document outlines the plan to upgrade the HP TouchPad's built-in browser from October 2009 WebKit to QtWebKit 2.3.4 (September 2014), bringing approximately 5 years of improvements to HTML5, CSS3, JavaScript performance, and security.

## Current State

| Component | Current Version | Target Version |
|-----------|-----------------|----------------|
| WebKit | ~r49000 (Oct 2009) | ~r156000 (Sep 2014) |
| JavaScript Engine | V8 2.5.9.22 | V8 (via bindings) |
| HTML Support | HTML5 basic | HTML5.1 |
| CSS Support | CSS 2.1 + partial CSS3 | Full CSS3 |
| TLS/SSL | OpenSSL 0.9.8k | OpenSSL 1.0.2u (separate update) |

## Why QtWebKit 2.3.4?

- **Last Qt 4.8 Compatible Version** - webOS uses Qt 4.8, and QtWebKit 2.3.4 is the last release that supports it
- **V8 Bindings Available** - Contains V8 JavaScript engine bindings from Chromium era (required for webOS performance)
- **Stable Release** - Well-tested, production quality code
- **Significant Improvements** - 5 years of security fixes, performance improvements, and web standards support

## Project Structure

```
webkit-upgrade/
├── qtwebkit-2.3.4.tar.gz           # Source archive (49MB)
├── browser-update-plan.md          # This document
├── UPGRADE-PLAN.md                 # Technical implementation plan
├── API-CHANGES.md                  # API differences documentation
├── extract-palm-files.sh           # Palm file extraction script
│
├── palm-extracted/                 # Extracted Palm platform files
│   ├── platform/palm/             # 149 core platform files
│   ├── platform/graphics/palm/    # 18 graphics layer files
│   ├── platform/network/palm/     # 26 network layer files
│   └── WebKit/palm/               # 73 WebKit API files
│       ├── Api/                   # Public API (palmwebview, etc.)
│       ├── WebCoreSupport/        # Chrome/Frame clients
│       └── webkit/                # Settings, clipboard, etc.
│
└── Source/                         # QtWebKit 2.3.4 with Palm port
    ├── WebCore/
    │   └── platform/
    │       ├── palm/              # Palm platform layer
    │       ├── graphics/palm/     # GPU acceleration (Piranha)
    │       └── network/palm/      # HTTP via libcurl
    ├── WebKit/palm/               # WebKit1 API for webOS
    ├── WebKit2/                   # Not used by webOS
    ├── JavaScriptCore/            # JSC (backup to V8)
    └── WTF/                       # Web Template Framework
```

## Palm Platform Port Files

### Total Files Extracted: 256

#### Platform Core (149 files)
Critical webOS integration files:

| File | Purpose |
|------|---------|
| `LunaServiceMgr.cpp/h` | Luna Service Bus connection |
| `PalmServiceBridge.cpp/h` | JavaScript ↔ Luna bridge |
| `V8PalmServiceBridgeCustom.cpp` | V8 bindings for services |
| `Sensor.cpp/h` | Accelerometer/gyroscope |
| `GeolocationServicePalm.cpp/h` | GPS integration |
| `RenderThemePalm.cpp/h` | webOS UI styling |
| `ClipboardPalm.cpp/h` | Copy/paste support |

#### Graphics Layer (18 files)
GPU-accelerated rendering via Piranha:

| File | Purpose |
|------|---------|
| `GraphicsLayerPalm.cpp/h` | Compositing layer |
| `LayerRendererPalm.cpp/h` | GPU rendering |
| `LayerPalm.cpp/h` | Base layer class |
| `ContentLayerPalm.cpp/h` | Content tiles |
| `VideoLayerPalm.cpp/h` | Video playback |
| `MediaPlayerPrivatePalm.cpp/h` | Media integration |

#### Network Layer (26 files)
HTTP/HTTPS via libcurl:

| File | Purpose |
|------|---------|
| `ResourceHandlePalm.cpp` | Core HTTP handling |
| `CurlHandle.cpp/h` | CURL wrapper |
| `CurlHandlePool.cpp/h` | Connection pooling |
| `AsyncLoader.cpp/h` | Async resource loading |
| `DiskCachePalm.cpp/h` | Disk caching |
| `SocketStreamHandlePalm.cpp` | WebSocket support |
| `CookieJarPalm.cpp` | Cookie storage |

#### WebKit API (73 files)
Public API and WebCore clients:

| Directory | Contents |
|-----------|----------|
| `Api/` | palmwebview, palmwebpage, palmwebframe |
| `WebCoreSupport/` | ChromeClient, FrameLoaderClient, EditorClient |
| `webkit/` | Settings, clipboard, timer utilities |

## Key API Changes (2009 → 2014)

### GraphicsLayer
```cpp
// OLD (2009)
GraphicsLayerPalm(GraphicsLayerClient*);

// NEW (2014) - Factory pattern
static PassOwnPtr<GraphicsLayer> create(GraphicsLayerFactory*, GraphicsLayerClient*);
```

### ResourceHandle
```cpp
// OLD (2009)
static PassRefPtr<ResourceHandle> create(const ResourceRequest&, ...);

// NEW (2014) - NetworkingContext added
static PassRefPtr<ResourceHandle> create(NetworkingContext*, const ResourceRequest&, ...);
```

### Animation Classes
```cpp
// OLD (2009)
AnimationValue(float keyTime, const TimingFunction* timingFunction = 0);

// NEW (2014) - PassRefPtr and clone()
AnimationValue(float keyTime, PassRefPtr<TimingFunction> timingFunction = 0);
virtual AnimationValue* clone() const = 0;
```

### Smart Pointers
More consistent use throughout:
- `PassOwnPtr<T>` - Transfer unique ownership
- `OwnPtr<T>` - Unique ownership
- `PassRefPtr<T>` - Transfer reference
- `RefPtr<T>` - Shared reference

## Implementation Phases

### Phase 1: Setup (COMPLETE)
- [x] Download QtWebKit 2.3.4 source
- [x] Extract Palm platform files from webOS patches
- [x] Create directory structure
- [x] Document API changes
- [x] Create build configuration

### Phase 2: Platform Core (~2-3 days)
- [ ] Update WTF includes and macros
- [ ] Add PLATFORM(PALM) defines
- [ ] Fix smart pointer usage patterns
- [ ] Port core platform files
- [ ] Test compilation

### Phase 3: Graphics Layer (~3-4 days)
- [ ] Update GraphicsLayerPalm to factory pattern
- [ ] Adapt animation API
- [ ] Port Piranha integration
- [ ] Test GPU compositing

### Phase 4: Network Layer (~2-3 days)
- [ ] Add NetworkingContext support
- [ ] Update ResourceHandlePalm
- [ ] Test with updated OpenSSL
- [ ] Verify HTTPS/TLS 1.2 works

### Phase 5: Luna Integration (~2-3 days)
- [ ] Port LunaServiceMgr
- [ ] Port PalmServiceBridge
- [ ] Update V8 bindings
- [ ] Test webOS app communication

### Phase 6: WebKit Shell (~3-5 days)
- [ ] Update ChromeClientPalm
- [ ] Update FrameLoaderClientPalm
- [ ] Port remaining WebCoreSupport clients
- [ ] Test page loading

### Phase 7: Build & Package (~2-3 days)
- [ ] Create ARM cross-compile configuration
- [ ] Build WebKit for TouchPad
- [ ] Create IPK update package
- [ ] Test on physical device

## Estimated Total Effort

| Phase | Effort | Skills Required |
|-------|--------|-----------------|
| Phase 2 | 2-3 days | C++, WebKit internals |
| Phase 3 | 3-4 days | OpenGL ES, compositing |
| Phase 4 | 2-3 days | Networking, curl |
| Phase 5 | 2-3 days | Luna services, V8 |
| Phase 6 | 3-5 days | WebKit architecture |
| Phase 7 | 2-3 days | ARM toolchain, packaging |
| **Total** | **2-3 weeks** | |

## Dependencies

### System Libraries Required
- Qt 4.8.0 (already on device)
- liblunaservice 2.0.0 (already on device)
- pbnjson 1.0.1 (already on device)
- Piranha 1.2 (already on device)
- libcurl 7.21.7 (already on device)
- OpenSSL 1.0.2u (via separate update package)

### Build Tools Required
- ARM cross-compiler (`arm-linux-gnueabi-gcc`)
- Qt 4.8 development headers
- Python 2.7 (for WebKit build scripts)
- Perl (for IDL code generation)
- Ruby (for some build scripts)

## Build Commands

### Configure
```bash
cd webkit-upgrade
export CROSS_COMPILE=arm-linux-gnueabi-
export QTDIR=/path/to/qt4.8-arm
export PKG_CONFIG_PATH=/path/to/arm-sysroot/usr/lib/pkgconfig

./Tools/Scripts/build-webkit --qt \
    --platform=palm \
    --release \
    --no-webkit2
```

### Compile
```bash
make -j$(nproc)
```

### Package
```bash
# Create IPK structure
mkdir -p package/CONTROL
mkdir -p package/usr/lib
mkdir -p package/usr/plugins/webkit

# Copy libraries
cp -a lib/libQtWebKit.so* package/usr/lib/
cp -a lib/webkit/* package/usr/plugins/webkit/

# Create control file and package
# ... (similar to OpenSSL package)
```

## Testing Plan

### Unit Tests
1. Compile test pages with new features
2. Test JavaScript execution
3. Test CSS3 rendering

### Integration Tests
1. Load standard webOS apps (Email, Browser)
2. Test Luna service calls from JavaScript
3. Test touch events and gestures
4. Test video playback

### Compatibility Tests
1. Test popular websites
2. Test HTML5 features (canvas, video, audio)
3. Test CSS3 animations and transforms

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Piranha API incompatibility | High | May need to adapt graphics layer significantly |
| V8 version mismatch | High | Fall back to JSC if needed |
| Luna service API changes | Medium | Should be stable, test thoroughly |
| Memory usage increase | Medium | Profile and optimize |
| Build system complexity | Low | Well-documented in WebKit |

## Rollback Plan

If the upgrade causes issues:
1. Keep original browser package backed up
2. Create restore script in IPK prerm
3. Document manual restore procedure

## Success Criteria

- [ ] Browser launches without crashes
- [ ] Basic web pages load correctly
- [ ] HTTPS sites work with TLS 1.2
- [ ] webOS apps function normally
- [ ] Touch gestures work
- [ ] Video playback works
- [ ] Performance acceptable (< 2x memory, similar speed)

## References

- [QtWebKit 2.3.4 Source](http://download.kde.org/stable/qtwebkit-2.3/2.3.4/src/qtwebkit-2.3.4.tar.gz)
- [QtWebKit GitHub](https://github.com/qtwebkit/qtwebkit)
- [WebKit Trac](https://trac.webkit.org/)
- [Qt 4.8 WebKit Documentation](https://doc.qt.io/archives/qt-4.8/qtwebkit-module.html)

## Changelog

- **2025-01-01**: Initial plan created
  - Downloaded QtWebKit 2.3.4
  - Extracted 256 Palm platform files
  - Documented API changes
  - Created build configuration
