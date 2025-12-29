
#include "config.h"
#include "GeolocationServicePalm.h"

#include "CString.h"
#include "GOwnPtr.h"
#include "NotImplemented.h"
#include "PositionOptions.h"
#include <sstream>
#include <pbnjson.hpp>
#include "webkitpalmsettings.h"

namespace WebCore {

const std::string GeolocationServicePalm::LOC_SERVICE = "palm://com.palm.location/";
const std::string GeolocationServicePalm::CUR_POSITION_API = "getCurrentPosition";
const std::string GeolocationServicePalm::START_TRACKING_API = "startTracking";


GeolocationService* GeolocationServicePalm::create(GeolocationServiceClient* client)
{
    return new GeolocationServicePalm(client);
}

GeolocationService::FactoryFunction* GeolocationService::s_factoryFunction = &GeolocationServicePalm::create;

GeolocationServicePalm::GeolocationServicePalm(GeolocationServiceClient* client)
    : GeolocationService(client)
    , m_latitude(0.0)
    , m_longitude(0.0)
    , m_altitude(0.0)
    , m_altitudeAccuracy(0.0)
    , m_heading(0.0)
    , m_speed(0.0)
    , m_accuracy(0.0)
    , m_timestamp(0.0)
    , m_serviceClient(0)
    , m_trackingRequestMsgToken(0)
{
}

GeolocationServicePalm::~GeolocationServicePalm()
{
    LSError lserror;
    LSErrorInit(&lserror);

    stopUpdating();

    if (m_serviceClient && !LSUnregister(m_serviceClient, &lserror)) {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }
}

bool GeolocationServicePalm::init()
{
	if( m_serviceClient )
		return true;
	
    LSError lserror;
    LSErrorInit(&lserror);

    if (!LSRegister(NULL, &m_serviceClient, &lserror)) {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
        setError(PERMANENT_ERROR);
        return false;
    }
    else {
        if (!LSGmainAttach(m_serviceClient, Palm::WebGlobal::mainLoop(), &lserror)) {
            LSErrorPrint(&lserror, stderr);
            LSErrorFree(&lserror);
            setError(PERMANENT_ERROR);
            return false;
        }
    }

	return true;	
}

bool GeolocationServicePalm::startUpdating(PositionOptions* options, bool singleFixReq)
{
	if( !init() )
		return false;
	
    m_lastPosition = 0;
    m_lastError = 0;

    // if tracking session is in progress there is no
    // need to request additional fixes
    if (m_trackingRequestMsgToken == 0) {
        LSError lserror;
        LSErrorInit(&lserror);
        std::ostringstream payload;

        // extract appId and processId of the calling app
        const char* clientId = getClientIdentifier();
        if (!clientId) {
			setError(UNKNOWN_ERROR);
			return false;
        }

        char clientIdentifier[strlen(clientId)+1];
        strcpy(clientIdentifier, clientId);
        char* appId = strtok(clientIdentifier, " ");
        char* processId = strtok(NULL, " ");
        if (!appId || !processId) {
        	setError(UNKNOWN_ERROR);
        	return false;
        }

		// get URL to send to location service
        String clientUrl;
        if (PalmBrowserSettings()->runningInBrowserServer) {
        	clientUrl = getClientUrl();
        	if (clientUrl.isEmpty()) {
				setError(UNKNOWN_ERROR);
				return false;
			}
		}

        // call getCurrentPosition first since it will return the fix sooner
        // appId and processId are required fields everything else is optional
        payload << "{\"appId\":\"";
        payload << appId << "\",\"processId\":\"" << processId << "\"";

        if (!clientUrl.isEmpty()) {
        	payload << ",\"url\":\"" << clientUrl.utf8().data() << "\"";
        }

        if (options->enableHighAccuracy()) {
            payload << ",\"accuracy\":1";
        }
        // From the spec: the default value used for the timeout attribute is Infinity.
        // If a negative value is supplied, the timeout value is considered to be 0.
        if (options->hasTimeout()) {
            payload << ",\"responseTime\":";
            int timeout = options->timeout() / 1000; // get seconds
            if (timeout < 5)
                payload << 1;
            else if (timeout < 20)
                payload << 2;
            else
                payload << 3;
        }
        // From the spec: if maximumAge is set to 0, the implementation must immediately attempt to
        // acquire a new position object. Setting the maximumAge to Infinity will force
        // the implementation to return a cached position regardless of its age.
        // if omitted, the default value used for the maximumAge attribute is 0.
        // If a negative value is supplied, the maximumAge value is considered to be 0
        if (options->hasMaximumAge()) {
            int maxAge = options->maximumAge();
            maxAge = maxAge < 0 ? 0 : maxAge;
            payload << ",\"maximumAge\":" << maxAge/1000;
        }
        payload << "}";

        if (!LSCall(m_serviceClient, (LOC_SERVICE + CUR_POSITION_API).c_str(),
                payload.str().c_str(), updateLocationInformation, (void*) this, NULL, &lserror))
        {
            LSErrorPrint(&lserror, stderr);
            LSErrorFree(&lserror);
            setError(UNKNOWN_ERROR);
            return false;
        }

        // start tracking if requested
        if (!singleFixReq) {
            payload.str(std::string());
            payload << "{\"subscribe\": true, \"appId\":\"";
            payload << appId << "\",\"processId\":\"" << processId << "\"";

            if (!clientUrl.isEmpty()) {
            	payload << ",\"url\":\"" << clientUrl.utf8().data() << "\"";
			}
            payload	<< "}";

            // start tracking
            if (!LSCall(m_serviceClient, (LOC_SERVICE + START_TRACKING_API).c_str(),
                    payload.str().c_str(), updateLocationInformation, (void*) this, &m_trackingRequestMsgToken, &lserror))
            {
                LSErrorPrint(&lserror, stderr);
                LSErrorFree(&lserror);
                m_trackingRequestMsgToken = 0;
                setError(UNKNOWN_ERROR);
                return false;
            }
        }
    }

    return true;
}

void GeolocationServicePalm::stopUpdating()
{
    if (m_serviceClient && m_trackingRequestMsgToken != 0) {
        // cancel tracking
        LSError lserror;
        LSErrorInit(&lserror);

        if (!LSCallCancel(m_serviceClient, m_trackingRequestMsgToken, &lserror)) {
            LSErrorPrint(&lserror, stderr);
            LSErrorFree(&lserror);
            setError(UNKNOWN_ERROR);
        }
        m_trackingRequestMsgToken = 0;
    }
}

void GeolocationServicePalm::suspend()
{
    return;
}

void GeolocationServicePalm::resume()
{
    return;
}

Geoposition* GeolocationServicePalm::lastPosition() const
{
    return m_lastPosition.get();
}

PositionError* GeolocationServicePalm::lastError() const
{
    return m_lastError.get();
}

bool GeolocationServicePalm::updateLocationInformation(LSHandle *sh, LSMessage *reply, void *ctx)
{
    GeolocationServicePalm*  that = (GeolocationServicePalm*) ctx;

    std::string jsonRaw = LSMessageGetPayload(reply);
    
    // sample payload
    // std::string jsonRaw("{\"errorCode\":0,\"timestamp\":1.268340607585E12,\"latitude\":37.390067,\"longitude\":-122.037626,
    // \"horizAccuracy\":150,\"heading\":0,\"velocity\":0,\"altitude\":0,\"vertAccuracy\":0}");

    pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment("{}");

    pbnjson::JDomParser parser;
    if (!parser.parse(jsonRaw, inputSchema)) {
    	that->setError(UNKNOWN_ERROR);
    	return true;
    }

    pbnjson::JValue json = parser.getDom();
    if (json.hasKey("errorCode")) {
    	int serviceErrCode = json["errorCode"].asNumber<int>();

		if (serviceErrCode == 0) {
			// everything is fine, get other args
			pbnjson::JValue lat = json["latitude"];
			pbnjson::JValue lng = json["longitude"];
			pbnjson::JValue alt = json["altitude"];
			pbnjson::JValue vacc = json["vertAccuracy"];
			pbnjson::JValue hacc = json["horizAccuracy"];
			pbnjson::JValue heading = json["heading"];
			pbnjson::JValue velocity = json["velocity"];
			pbnjson::JValue time = json["timestamp"];

			if (!lat.isNull() && !lng.isNull() && !alt.isNull() && !vacc.isNull()
				&& !hacc.isNull() && !heading.isNull() && !velocity.isNull() && !time.isNull()) {

				that->m_latitude = lat.asNumber<double>();
				that->m_longitude = lng.asNumber<double>();
				that->m_altitude = alt.asNumber<double>();
				that->m_altitudeAccuracy = vacc.asNumber<double>();
				that->m_accuracy = hacc.asNumber<double>();
				that->m_heading = heading.asNumber<double>();
				that->m_speed = velocity.asNumber<double>();
				that->m_timestamp = time.asNumber<double>();

				that->updatePosition();
			}
			else {
				that->setError(UNKNOWN_ERROR);
			}
		}
		else {
			that->setError(LOC_SERVICE_ERROR, serviceErrCode);
		}
    }
    else if (json.hasKey("returnValue") && !json["returnValue"].asBool()) {
    	that->setError(UNKNOWN_ERROR);
    }

    return true;
}

void GeolocationServicePalm::updatePosition()
{
    m_lastError = 0;

    RefPtr<Coordinates> coordinates = Coordinates::create(m_latitude, m_longitude,
        true, m_altitude, m_accuracy, true, m_altitudeAccuracy, true, m_heading, true, m_speed);

    DOMTimeStamp time = (DOMTimeStamp)(m_timestamp);
    m_lastPosition = Geoposition::create(coordinates.release(), time);
    positionChanged();
}

void GeolocationServicePalm::setError(GeolocationServicePalm::ErrorType errType, int locServiceErr)
{
    m_lastPosition = 0;
    PositionError::ErrorCode errorCode;
    std::string msg;

    if (errType == GeolocationServicePalm::LOC_SERVICE_ERROR) {
        // translate location service errors
        if (locServiceErr == 1) {
            errorCode = PositionError::TIMEOUT;
            msg = "Timeout expired";
        }
        else if (locServiceErr == 2) {
            errorCode = PositionError::POSITION_UNAVAILABLE;
            msg = "Position is not available";
        }
        else {
            errorCode = PositionError::POSITION_UNAVAILABLE;
            msg = "Unknown error";
        }
    }
    else {
        errorCode = PositionError::POSITION_UNAVAILABLE;
        msg = "Unknown error";
    }

    m_lastError = PositionError::create(errorCode, String::fromUTF8(msg.c_str()));
    if (errType == GeolocationServicePalm::PERMANENT_ERROR) {
        m_lastError.get()->setIsFatal(true);
    }

    errorOccurred();
}

}
