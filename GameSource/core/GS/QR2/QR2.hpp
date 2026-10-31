#ifndef _QR2_
#define _QR2_
#include <types.hpp>

extern "C" {
BOOL qr2_buffer_addA(void *buffer, const char *value);  // 8010f434
BOOL qr2_keybuffer_add(void *buffer, int key);  // 8010f354
int qr2_init_socketA(void *q, int socket, int boundPort, const char *gameName, const char *secretKey, int isPublic, int natNegotiate, void *serverKeyCallback, void *playerKeyCallback,
  void *teamKeyCallback, void *keyListCallback, void *countCallback, void *addErrorCallback, void *userdata);  // 8010ecac
}

// GameSpy Query & Reporting 2
namespace QR2 {
extern const char hostNameFmt[];  // 8027d084
}  // namespace QR2

#endif