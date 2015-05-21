#ifndef NID_SERVER_H
#define NID_SERVER_H

#include <ve_clock.h>
#include <nid.h>
#include <scan.h>

struct nid_server_driver;
struct nid_server_device;

extern int nid_server_fd;

typedef struct nid_server_driver {
  char *name;
  void (*init)(struct nid_server_driver *);
  void (*report)(struct nid_server_device *, int on, int elemid);
  void (*destroy)(struct nid_server_device *);
  /* handle other request - can be NULL if nothing supported */
  void (*func)(struct nid_server_device *, NidPacket *pkt);
} NidServerDriver;

typedef struct nid_server_device {
  struct nid_server_driver *driver;
  char *name;
  char *type;
  int devid;
  int nelems;
  NidElementInfo *info;
  NidElementState *state;
  void *priv; /* driver-specific private data */
} NidServerDevice;

void nidServer_addDevice(NidServerDevice *);
NidServerDevice *nidServer_findDevice(int devid);
int nidServer_uniqueId();
void nidServer_nak(NidPacket *, int fd, int reason);
void nidServer_ack(NidPacket *, int fd);
void nidServer_addEvent(NidElementState *s);

#endif /* NID_SERVER_H */
