
#ifndef __DiskCacheDisk_h__
#define __DiskCacheDisk_h__

#include "DiskCachePalm.h"

#include <string>
#include <vector>

namespace WebCore {

class DiskCacheDisk : public DiskCache
{
public:
	DiskCacheDisk();
	~DiskCacheDisk();
	
	virtual void			clear();
	virtual DiskCacheEntry*	contains( const String& url );
	virtual DiskCacheEntry* create( );
		
	virtual void			remove( const String& url );
	virtual void			remove( DiskCacheEntry* );
	virtual void			add( DiskCacheEntry* );
	virtual void			schedule( DiskCacheEntry* );
	virtual void			cancelScheduled( ResourceHandle*  );
	virtual void			setMaxCapacity( unsigned long sz, bool prune = true );
	
	void			statistics();
	void 			serviceRequestsTimerFired(Timer<DiskCacheDisk>*);	
	
private:
	void		init();
	bool		pruneToFit( unsigned long targetSize );	
	void		inventoryDiskCache();
	
	// These two are separated because the HashMap doesn't support
	// an erase operation with a  non-modifiable iterator.
	
	
	unsigned long m_maxSizeBytes;
	unsigned long m_currentSizeBytes;	
	Timer<DiskCacheDisk> m_serviceRequestsTimer;
	Vector<DiskCacheEntry*> m_scheduled;
	String m_diskCachePath;
	
    struct ScoredFile {
        std::string m_name;
        int m_score;

        ScoredFile(char *name, int score) : m_name(name), m_score(score) {}
        bool operator<(const ScoredFile &rhs) const {
            return m_score > rhs.m_score; // use > instead of < so the list will be in reverse order
        }
    };

    void pruneExpiredAndScore();
    bool pruneLowestScore();

    time_t m_lastPruneExpiredAndScore;
    std::vector<ScoredFile> m_scores;
};


}

#endif // __DiskCacheDisk_h__
