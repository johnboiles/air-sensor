// Hack because RemoteDebug.h uses `uint8` for some reason
#include <inttypes.h>
#define uint8 uint8_t

#include <RemoteDebug.h>

#if LOGGING
RemoteDebug Debug;
#endif
