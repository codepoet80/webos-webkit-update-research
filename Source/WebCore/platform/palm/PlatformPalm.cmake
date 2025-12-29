# Palm/webOS Platform Configuration for QtWebKit 2.3.4
#
# This file configures the Palm platform port for webOS 3.0.5

# Platform identification
set(PLATFORM_PALM 1)
set(WTF_PLATFORM_PALM 1)

# Required feature flags
set(USE_ACCELERATED_COMPOSITING 1)
set(ENABLE_VIDEO 1)
set(ENABLE_GEOLOCATION 1)
set(ENABLE_DEVICE_ORIENTATION 1)
set(ENABLE_TOUCH_EVENTS 1)

# Disabled features (not implemented for Palm)
set(ENABLE_CSS_FILTERS 0)
set(ENABLE_CSS_SHADERS 0)
set(ENABLE_WEBGL 0)  # Could be enabled if Piranha supports it

# JavaScript engine - use V8 for webOS compatibility
set(USE_V8 1)
set(USE_JSC 0)

# Network layer
set(USE_CURL 1)

# Graphics
set(USE_PIRANHA 1)  # Palm's graphics library

# Palm-specific source files
list(APPEND WebCore_SOURCES
    # Platform core
    platform/palm/CachedImagePalm.cpp
    platform/palm/CachePalm.cpp
    platform/palm/ClipboardCommands.cpp
    platform/palm/ClipboardController.cpp
    platform/palm/ClipboardPalm.cpp
    platform/palm/ClipboardWidgetController.cpp
    platform/palm/ClipboardWidget.cpp
    platform/palm/ContextMenuItemPalm.cpp
    platform/palm/ContextMenuPalm.cpp
    platform/palm/CookieJarPalm.cpp
    platform/palm/CookieLruCachePalm.cpp
    platform/palm/CookieServicePalm.cpp
    platform/palm/CursorPalm.cpp
    platform/palm/DatabaseTrackerPalm.cpp
    platform/palm/DeviceMotionClientPalm.cpp
    platform/palm/DeviceMotionProviderPalm.cpp
    platform/palm/DeviceOrientationClientPalm.cpp
    platform/palm/DeviceOrientationProviderPalm.cpp
    platform/palm/EventLoopPalm.cpp
    platform/palm/FileChooserPalm.cpp
    platform/palm/FileSystemPalm.cpp
    platform/palm/FontCachePalm.cpp
    platform/palm/FontDataPalm.cpp
    platform/palm/GeolocationServicePalm.cpp
    platform/palm/KeyEventPalm.cpp
    platform/palm/KURLPalm.cpp
    platform/palm/Language.cpp
    platform/palm/LocalizedStringsPalm.cpp
    platform/palm/LoggingPalm.cpp
    platform/palm/LunaResources.cpp
    platform/palm/LunaServiceMgr.cpp
    platform/palm/MIMETypeRegistryPalm.cpp
    platform/palm/PalmServiceBridge.cpp
    platform/palm/PasteboardPalm.cpp
    platform/palm/PlatformBridgePalm.cpp
    platform/palm/PlatformScreenPalm.cpp
    platform/palm/PlatformScrollBarPalm.cpp
    platform/palm/PlatformTouchEventPalm.cpp
    platform/palm/PlatformTouchPointPalm.cpp
    platform/palm/PopupMenuPalm.cpp
    platform/palm/RenderThemePalm.cpp
    platform/palm/ScrollbarPalm.cpp
    platform/palm/ScrollbarThemePalm.cpp
    platform/palm/ScrollViewPalm.cpp
    platform/palm/SearchPopupMenuPalm.cpp
    platform/palm/Sensor.cpp
    platform/palm/SensorManager.cpp
    platform/palm/SensorObjects.cpp
    platform/palm/SharedBufferPalm.cpp
    platform/palm/SharedTimerPalm.cpp
    platform/palm/SingletonTimerPalm.cpp
    platform/palm/SoundPalm.cpp
    platform/palm/SystemTimePalm.cpp
    platform/palm/TemporaryLinkStubs.cpp
    platform/palm/ThreadingPalm.cpp
    platform/palm/WidgetPalm.cpp

    # Graphics layer
    platform/graphics/palm/BasicLayerPalm.cpp
    platform/graphics/palm/CanvasLayerPalm.cpp
    platform/graphics/palm/ContentLayerPalm.cpp
    platform/graphics/palm/GraphicsLayerPalm.cpp
    platform/graphics/palm/ImageLayerPalm.cpp
    platform/graphics/palm/LayerPalm.cpp
    platform/graphics/palm/LayerRendererPalm.cpp
    platform/graphics/palm/MediaPlayerPrivatePalm.cpp
    platform/graphics/palm/VideoLayerPalm.cpp

    # Network layer
    platform/network/palm/AsyncLoader.cpp
    platform/network/palm/AuthenticationHandler.cpp
    platform/network/palm/CredentialsCache.cpp
    platform/network/palm/CurlHandle.cpp
    platform/network/palm/CurlHandlePool.cpp
    platform/network/palm/DirectLoader.cpp
    platform/network/palm/DiskCacheDisk.cpp
    platform/network/palm/DiskCachePalm.cpp
    platform/network/palm/DNSPalm.cpp
    platform/network/palm/NetworkStateNotifierPalm.cpp
    platform/network/palm/ResourceHandlePalm.cpp
    platform/network/palm/SocketStreamHandlePalm.cpp
)

# V8 bindings for Palm
if(USE_V8)
    list(APPEND WebCore_SOURCES
        platform/palm/V8PalmServiceBridgeCustom.cpp
        platform/palm/V8SensorCustom.cpp
        platform/palm/V8SensorManagerCustom.cpp
        platform/palm/V8DOMWindowCustomPalm.cpp
    )
endif()

# Include directories
list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/palm"
    "${WEBCORE_DIR}/platform/graphics/palm"
    "${WEBCORE_DIR}/platform/network/palm"
)

# Link libraries
list(APPEND WebCore_LIBRARIES
    lunaservice     # Luna Service Bus
    pbnjson         # JSON parsing
    piranha         # Palm graphics
    curl            # HTTP client
    glib-2.0        # GLib
)

# Compiler definitions
list(APPEND WebCore_DEFINITIONS
    -DPLATFORM_PALM=1
    -DBUILDING_PALM__=1
    -DUSE_ACCELERATED_COMPOSITING=1
)
