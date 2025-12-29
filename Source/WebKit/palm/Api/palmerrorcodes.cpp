#include <assert.h>
#include "palmerrorcodes.h"
#include <curl/curl.h>


namespace Palm {

/**
 * Map a system error (errno) to a Palm error code.
 * @see IEEE Std 1003.1-2001.
 */
int MapErrnoToPalmError(int errNo)
{
	if (errNo >= 0 && errNo <= ERR_SYS_LAST_ERROR) {
		return errNo;
	}
	else {
		return ERR_SYS_FAILURE;
	}
}

/**
 * Map a CURL error code to a Palm error code.
 */
int MapCurlErrToPalmError(CURLcode errNo)
{
	if (errNo >= CURLE_OK && errNo < CURL_LAST) {
		return static_cast<int>(errNo) + ERR_CURL_FIRST_ERROR;
	}
	else {
		// Either CURL added error codes w/o changing CURL_LAST or user passed
		// in a value that was not a CURL error code.
		assert(false);
		return ERR_CURL_FAILURE;
	}
}

} 
