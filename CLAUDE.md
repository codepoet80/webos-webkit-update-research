# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a research project for updating the HP TouchPad's webOS browser. It contains:
- QtWebKit 2.3.4 source code (September 2014) as the upgrade target
- Palm/webOS platform port files extracted from the original October 2009 WebKit
- Security patches addressing CVEs from 2009-2013
- Original webOS browser source with Palm modifications

## Repository Structure

```
├── Source/                    # QtWebKit 2.3.4 (upgrade target)
│   ├── WebCore/              # Core rendering engine
│   ├── WebKit/palm/          # Palm port API layer (Api/, WebCoreSupport/, webkit/)
│   ├── JavaScriptCore/       # JavaScript engine
│   ├── WTF/                  # Web Template Framework
│   └── WebKit2/              # Not used by webOS
├── browser-src/              # Original webOS WebKit with Palm patches
│   ├── webcore-patch.diff    # 74,873 lines of Palm modifications
│   ├── webkit-patch.diff     # 27,745 lines of Palm modifications
│   └── security-patches/     # Security fixes (same as root)
├── palm-extracted/           # Extracted Palm platform files (256 files)
│   ├── platform/palm/        # Luna service integration, sensors
│   ├── platform/graphics/    # GPU acceleration via Piranha
│   └── platform/network/     # HTTP via libcurl
└── security-patches/         # CVE patches (001-005)
```

## Key webOS APIs to Preserve

- **PalmServiceBridge**: JavaScript ↔ Luna Service Bus bridge for system services
- **LunaServiceMgr**: Connection to Luna Service Bus
- **PalmSystem**: JavaScript global object for webOS apps
- **Sensor/SensorManager**: Accelerometer/gyroscope APIs

## Build Commands

### Configure for Cross-Compilation
```bash
export CROSS_COMPILE=arm-linux-gnueabi-
export QTDIR=/path/to/qt4.8-arm
export PKG_CONFIG_PATH=/path/to/arm-sysroot/usr/lib/pkgconfig

./Tools/Scripts/build-webkit --qt --platform=palm --release --no-webkit2
```

### Build
```bash
make -j$(nproc)
```

### Apply Security Patches
```bash
cd browser-src
./security-patches/apply-patches.sh

# Or dry-run first:
./security-patches/apply-patches.sh --dry-run

# Apply single patch:
patch -p1 < security-patches/001-rendering-null-checks.patch
```

## Dependencies

Required on device (already present on TouchPad):
- Qt 4.8.0, liblunaservice 2.0.0, pbnjson 1.0.1
- Piranha 1.2 (Palm graphics library), libcurl 7.21.7
- OpenSSL 1.0.2u (via separate update package for TLS 1.2)

Build tools required:
- ARM cross-compiler (arm-linux-gnueabi-gcc)
- Qt 4.8 development headers
- Python 2.7, Perl, Ruby (for WebKit build scripts)

## Critical Implementation Notes

### Smart Pointer Migration (2009 → 2014 API)
The upgrade requires updating Palm files to use new WebKit smart pointer patterns:
```cpp
// OLD: Direct construction
GraphicsLayerPalm(GraphicsLayerClient*);

// NEW: Factory pattern with PassOwnPtr
static PassOwnPtr<GraphicsLayer> create(GraphicsLayerFactory*, GraphicsLayerClient*);
```

### NetworkingContext
ResourceHandle now requires NetworkingContext as first parameter.

### Platform Macro
Palm platform files use `PLATFORM(PALM)` conditional compilation.

## Security Patches

| Patch | CVEs | Issue |
|-------|------|-------|
| 001 | CVE-2010-1813 | Null pointer in rendering |
| 002 | CVE-2011-0222, CVE-2011-0240 | SVG use-after-free |
| 003 | CVE-2010-0046 | CSS memory corruption |
| 004 | CVE-2009-1684, CVE-2009-1695 | XSS vulnerabilities |
| 005 | CVE-2010-0049 | RTL text use-after-free |
