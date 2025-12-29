#ifndef MediaPlayerPrivatePalm_h
#define MediaPlayerPrivatePalm_h

#if ENABLE(VIDEO)

#include "HTMLMediaElement.h"
#include "Image.h"
#include "MediaPlayerPrivate.h"
#include "PGContext.h"
#include "RefPtr.h"
#include "TimeRanges.h"

#include <lunaservice.h>
#include <media/Clonk.h>
#include <media/LunaConnector.h>
#include <media/MediaCaptureV3.h>
#include <media/MediaClient.h>
#include <media/MediaPlayer.h>
#include <pbnjson.hpp>
#include <remoteplugin/RemotePlugin.h>

class PGSurface;


namespace WTF {
class String;
}

namespace WebCore { namespace palm {
class MediaChangeListener;
class MediaPlayerChangeListener;
class MediaCaptureV3ChangeListener;
class ClonkChangeListener;
} // namespace palm

class IntSize;
class IntRect;
class VideoLayerPalm;


class MediaPlayerPrivate : public MediaPlayerPrivateInterface, media::LunaConnector, RemotePluginListener {
    friend class WebCore::palm::MediaChangeListener;
    friend class WebCore::palm::MediaPlayerChangeListener;
    friend class WebCore::palm::MediaCaptureV3ChangeListener;
    friend class WebCore::palm::ClonkChangeListener;

public:
    static void registerMediaEngine(MediaEngineRegistrar);
    virtual ~MediaPlayerPrivate();

    virtual void load(const String& url);
    virtual void cancelLoad();
    virtual void play();
    virtual void pause();
    virtual void disconnect();
    virtual void pageFocus(bool focus);
    virtual void setFitMode(const String& mode);

    virtual IntSize naturalSize() const;

    virtual bool hasVideo() const;
    virtual bool hasAudio() const;

    virtual void setVisible(bool);
    virtual float duration() const;

    virtual float currentTime() const;
    virtual void seek(float time);
    virtual bool seeking() const;

    virtual void setEndTime(float);
    virtual void setRate(float);

    virtual bool paused() const;
    virtual void setVolume(float);

    virtual MediaPlayer::NetworkState networkState() const;
    virtual MediaPlayer::ReadyState readyState() const;

    virtual float maxTimeSeekable() const;
    virtual PassRefPtr<TimeRanges> buffered() const;

    virtual int dataRate() const;

    virtual unsigned totalBytes() const;
    virtual unsigned bytesLoaded() const;

    virtual void setSize(const IntSize&);
    virtual void paint(GraphicsContext*, const IntRect&);

    void releaseBuffer();

    bool hasSingleSecurityOrigin() const { return true; }

#if USE(ACCELERATED_COMPOSITING)
    // whether accelerated rendering is supported by the media engine for the current media.
    virtual bool supportsAcceleratedRendering() const;
    // called when the rendering system flips the into or out of accelerated rendering mode.
    virtual void acceleratedRenderingStateChanged();
    // returns an object that can be directly composited via GraphicsLayerQt (essentially a QGraphicsItem*)
    virtual PlatformLayer* platformLayer() const;

    int mapBufferTexture(int textureId[]);

#endif

private:
    MediaPlayerPrivate(MediaPlayer* player);
    static MediaPlayerPrivateInterface* create(MediaPlayer* player);

    static void getSupportedTypes(HashSet<String>&);
    static MediaPlayer::SupportsType supportsType(const String& type, const String& codecs);
    static bool isAvailable() { return true; }

    static HashSet<String> GetTypeCache();

    virtual void draw();
    bool connectIfRequired();
    void init();
    void deinit();

    void update();
    bool lockBuffer(RemotePluginBuffer* buffer, bool invalidateCache);
    void unlockBuffer();
    void captureRemoteBuffer();

    void renderFrame(PGContext* c, PGSurface* src, const IntRect& rect);

    Image* getPlayButtonImage();

    int* getVideoFrameCaptureData(int width, int height);

    // MediaConnector methods
    virtual LSPalmService* connectToBus();
    virtual void connected();

private:
    MediaPlayer* m_player;
    RemotePlugin m_remote;
    RemotePluginParams m_remoteParams;
    int m_currentRemoteBuffer;
    RemotePluginBuffer m_currentRemoteBufferHold;
    int m_updatesPending;
    float m_remoteBufferScale;
    float m_lastGcScale;
    bool m_remoteBufferResizePending;
    HTMLMediaElement* m_element;

    PGSurface *        m_videoFrameCaptured;

    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;
    bool m_readyForDisplay;

    boost::shared_ptr<media::MediaPlayer> m_client;
    boost::shared_ptr<WebCore::palm::MediaPlayerChangeListener> m_listener;

    boost::shared_ptr<media::MediaCaptureV3> m_capture;
    boost::shared_ptr<WebCore::palm::MediaCaptureV3ChangeListener> m_capturelistener;
            
    boost::shared_ptr<media::Clonk> m_clonk;
    boost::shared_ptr<WebCore::palm::ClonkChangeListener> m_clonklistener;

    LSPalmService* m_serverHandle;

    int m_bytesLoaded;
    int m_totalBytes;
    float m_currentTime;
    float m_duration;
    bool m_paused;
    bool m_seeking;
    bool m_hasAudio;
    bool m_hasVideo;
    bool m_unloading;

    HashSet<String> typeCache;
    String m_lastLoadingUri;
    bool m_loadPendingUri;
    std::string m_currentPlayer;

    bool m_overlayplayback;
    int m_width;
    int m_height;
    media::VideoFitMode m_fitMode;

    bool m_pausable;

    // picture-in-picture related member variables
    IntRect m_pipRect;
    bool m_pipVideoActive;
    bool m_fullVideoActive;

#if USE(ACCELERATED_COMPOSITING)
    RefPtr<VideoLayerPalm> m_videoLayer;
    bool m_composited;
#endif

};
} // namespace WebCore

#endif // ENABLE(VIDEO)
#endif // MediaPlayerPrivatePalm_h

