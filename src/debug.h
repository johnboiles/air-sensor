#if LOGGING
#include <RemoteDebug.h>
#define DLOG(msg, ...) if(Debug.isActive(Debug.DEBUG)){Debug.printf(msg, ##__VA_ARGS__);}
extern RemoteDebug Debug;
#else
#define DLOG(msg, ...)
#endif
