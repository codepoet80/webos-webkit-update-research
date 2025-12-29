
#ifndef GeolocationServicePalm_h
#define GeolocationServicePalm_h

#include "GeolocationService.h"
#include "Geoposition.h"
#include "PositionError.h"
#include "RefPtr.h"
#include <lunaservice.h>
#include "palmwebglobal.h"

namespace WebCore {

class GeolocationServicePalm : public GeolocationService {
public:
    static GeolocationService* create(GeolocationServiceClient*);
    ~GeolocationServicePalm();

    virtual bool startUpdating(PositionOptions*, bool);
    virtual void stopUpdating();

    virtual void suspend();
    virtual void resume();

    Geoposition* lastPosition() const;
    PositionError* lastError() const;

private:
    bool init();

    static const std::string LOC_SERVICE;
    static const std::string CUR_POSITION_API;
    static const std::string START_TRACKING_API;

    enum ErrorType {
        PERMANENT_ERROR = 1,
        UNKNOWN_ERROR = 2,
        LOC_SERVICE_ERROR = 3,
    };

    void setError(ErrorType errCode, int locServiceErr = 0);
    void updatePosition();

    static bool updateLocationInformation(LSHandle *handle, LSMessage *reply, void *ctx);

private:

    GeolocationServicePalm(GeolocationServiceClient*);

    RefPtr<Geoposition> m_lastPosition;
    RefPtr<PositionError> m_lastError;

    // Error and Position state
    double m_latitude;
    double m_longitude;
    double m_altitude;
    double m_accuracy;
    double m_altitudeAccuracy;
    double m_heading;
    double m_speed;
    double m_timestamp;

    LSHandle* m_serviceClient;
    LSMessageToken m_trackingRequestMsgToken;

};
}

#endif
