#include "config.h"
#include "palmfpshandler.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace Palm{

PalmFpsHandler::PalmFpsHandler(char fileName[])
    : m_nCount(0)
    , m_previousTimeMs(0)
    , m_nFps(0)
    , m_nAverageFps(0)
    , m_nStandardDeviation(0)
    , m_nTotalDeviation(0)
    , m_fLog(fopen(fileName, "a"))
{
    if (!m_fLog)
        g_warning("FPS File un-writable - not writing to file");
}

PalmFpsHandler::~PalmFpsHandler()
{
    if (m_fLog)
        fclose(m_fLog);
}

void PalmFpsHandler::framePainted(long int elapsedTimeMs)
{

    if (m_nCount > FPS_SAMPLING_INTERVAL || elapsedTimeMs - m_previousTimeMs > MAX_TIME_INTERVAL) {
        if (m_nCount > 1)
            writeFile();
    } else {
        m_nFps = 1000.0/(elapsedTimeMs - m_previousTimeMs);

        // Get the standard deviation based off previous average
        m_nTotalDeviation = m_nTotalDeviation + ((m_nCount - 1.0) / m_nCount) * (m_nFps - m_nAverageFps) * (m_nFps - m_nAverageFps);
        m_nStandardDeviation = sqrt(m_nTotalDeviation/m_nCount);

        // Get the next running average
        m_nAverageFps = m_nAverageFps + (m_nFps - m_nAverageFps)/m_nCount;

    }

    m_previousTimeMs = elapsedTimeMs;
    m_nCount++;
}

void PalmFpsHandler::writeFile()
{
    if (m_fLog) {
        fprintf(m_fLog, "[%ld s] Fps %Lf.2 SD %Lf.2 \n", m_previousTimeMs/1000, m_nAverageFps, m_nStandardDeviation);
        fflush(m_fLog);
    }
    m_nCount = 0;
    m_nAverageFps = 0;
    m_nTotalDeviation = 0;
}

}
