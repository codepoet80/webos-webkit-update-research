
#include "config.h"
#include "PlatformString.h"
#include "CString.h"
#include "KURL.h"


namespace WebCore
{

String KURL::fileSystemPath() const 
{ 
/*
	printf("host=%s\n",host().utf8().data() );
	printf("path=%s\n",path().utf8().data() );
	printf("lastPathComponent=%s\n",lastPathComponent().utf8().data() );
	printf("prettyURL=%s\n",prettyURL().utf8().data() );
	printf("string=%s\n",string().utf8().data() );
	printf("hasPath=%d\n",hasPath() );
	printf("isLocalFile=%d\n",isLocalFile() );
	printf("\n");
	*/
	if( !isLocalFile() )
		return String();
	
	String s = string();
	if( s.startsWith("file://") ) {
		s = decodeURLEscapeSequences(s.substring(7));
	}
	
	return s;
}

}
