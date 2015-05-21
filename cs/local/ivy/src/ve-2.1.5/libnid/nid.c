/*
 * provides general cross-platform support
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <stddef.h>
#include <nid.h>

/* we are unlikely to need very many connections in one invocation - if
   so, just up this limit */
#define MAXCONNS 32
/* maximum number of packets to queue up */
typedef struct nid_pkt_buf {
  NidPacket *packet;
  struct nid_pkt_buf *next;
} NidPacketBuffer;

static struct nid_connection {
  int in_use;
  int socket;              /* as given to us by OS layer */
  int evsocket;            /* from OS layer - where to send events */
  int evtport;
  int next_request;        /* next request index to transmit */
  int last_err;
  char *last_msg;
} connections[MAXCONNS];

int nidConnectionFd(int i) {
  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);
  return connections[i].socket;
}

int nidEventFd(int i) {
  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);
  return connections[i].evsocket;
}

static int next_request(int i) {
  int req;

  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);
  
  req = connections[i].next_request++;
  if (req == 0) /* never return 0 - it is reserved */
    req = connections[i].next_request++;
  return req;
}

static int log_to_syslog = 0; /* by default we go to stderr */
void _nidLogMsg(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  if (log_to_syslog)
    vsyslog(LOG_ERR,fmt,ap);
  else {
    fprintf(stderr, "NID: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
  }
  va_end(ap);
}

static char *pkt_to_string(int type) {
  switch(type) {
  case NID_PKT_HANDSHAKE: return "HANDSHAKE";
  case NID_PKT_ACK: return "ACK";
  case NID_PKT_NAK: return "NAK";
  case NID_PKT_ENUM_DEVICES: return "ENUM_DEVICES";
  case NID_PKT_ENUM_ELEMENTS: return "ENUM_ELEMENTS";
  case NID_PKT_QUERY_ELEMENTS: return "QUERY_ELEMENTS";
  case NID_PKT_LISTEN_ELEMENTS: return "LISTEN_ELEMENTS";
  case NID_PKT_IGNORE_ELEMENTS: return "IGNORE_ELEMENTS";
  case NID_PKT_DEVICE_LIST: return "DEVICE_LIST";
  case NID_PKT_ELEMENT_LIST: return "ELEMENT_LIST";
  case NID_PKT_ELEMENT_STATES: return "ELEMENT_STATES";
  case NID_PKT_ELEMENT_EVENTS: return "ELEMENT_EVENTS";
  case NID_PKT_SET_VALUE: return "SET_VALUE";
  case NID_PKT_GET_VALUE: return "GET_VALUE";
  case NID_PKT_RETURN_VALUE: return "RETURN_VALUE";
  case NID_PKT_FIND_DEVICE: return "FIND_DEVICE";
  case NID_PKT_DEVICE_FUNC: return "DEVICE_FUNC";
  case NID_PKT_DEVICE_RESP: return "DEVICE_RESP";
  case NID_PKT_TIME_SYNCH: return "TIME_SYNCH";
  case NID_PKT_COMPRESS_EVENTS: return "COMPRESS_EVENTS";
  case NID_PKT_UNCOMPRESS_EVENTS: return "UNCOMPRESS_EVENTS";
  case NID_PKT_DUMP_EVENTS: return "DUMP_EVENTS";
  case NID_PKT_SET_EVENT_SINK: return "SET_EVENT_SINK";
  case NID_PKT_EVENTS_AVAIL: return "EVENTS_AVAIL";
  }
  return "unknown";
}

static void logpkt(char *note, NidPacket *pkt) {
  _nidLogMsg("%s packet: %d %s", note, pkt->header.request, 
	 pkt_to_string(pkt->header.type));
}

void nidLogToSyslog(int bool) {
  log_to_syslog = bool;
}

void nidFreePacket(NidPacket *p) {
  if (p) {
    if (p->payload) {
      switch(p->header.type) {
      case NID_PKT_QUERY_ELEMENTS:
      case NID_PKT_LISTEN_ELEMENTS:
      case NID_PKT_IGNORE_ELEMENTS:
	nidFreeElementRequests((NidElementRequests *)p->payload);
	break;
      case NID_PKT_DEVICE_LIST:
	nidFreeDeviceList((NidDeviceList *)p->payload);
	break;
      case NID_PKT_ELEMENT_LIST:
	nidFreeElementList((NidElementList *)p->payload);
	break;
      case NID_PKT_ELEMENT_STATES:
      case NID_PKT_ELEMENT_EVENTS:
	nidFreeStateList((NidStateList *)p->payload);
	break;
      default:
	free(p->payload);
      }
    }
    free(p);
  }
}

static void free_packet_buf(NidPacketBuffer *buf) {
  NidPacketBuffer *tmp;
  while(buf) {
    tmp = buf->next;
    nidFreePacket(buf->packet);
    buf = tmp;
  }
}

static int alloc_conn() {
  int i;
  for(i = 0; i < MAXCONNS; i++)
    if (!connections[i].in_use) {
      connections[i].in_use = 1;
      connections[i].socket = -1;
      connections[i].evsocket = -1;
      connections[i].next_request = 1; /* 0 is reserved */
      connections[i].last_err = 0;
      connections[i].last_msg = NULL;
      return i;
    }
  _nidLogMsg( "Maximum number of connections (%d) exceeded\n", MAXCONNS);
  abort();
  return -1;
}

static void free_conn(int i) {
  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);
  /*  free_packet_buf(connections[i].head); */
  connections[i].in_use = 0;
}

#define UNPACK_PKTTYPE(x) x *out = malloc(sizeof(x)); assert(out != NULL); pkt->payload = out;
#define UNPACK_STR(dst,c) { strncpy((dst),(c),NID_DEVICE_STR_SZ); \
(dst)[NID_DEVICE_STR_SZ-1] = '\0'; \
(c) += NID_DEVICE_STR_SZ; }
#define UNPACK_SSTR(dst,c) { strncpy((dst),(c),NID_SINK_STR_SZ); \
(dst)[NID_SINK_STR_SZ-1] = '\0'; \
(c) += NID_SINK_STR_SZ; }
#define UNPACK_UINT32(dst,c) { (dst) = nidOs_n2h_uint32(*(nid_uint32 *)(c)); \
(c) += sizeof(nid_uint32); }
#define UNPACK_INT32(dst,c) { (dst) = nidOs_n2h_int32(*(nid_int32 *)(c)); \
(c) += sizeof(nid_int32); }
#define UNPACK_REAL(dst,c) { (dst) = nidOs_n2h_real(*(nid_real *)(c)); \
(c) += sizeof(nid_real); }

/* receive a packet for a particular request */
static NidPacket *nidReceiveRawPacket(int tport, int fd, int req, int wait) {
  NidPacket *pkt;
  char *pbuf, *c;
  int len;

  /* skip packets that are not for my request - this is not necessarily good
     - for multithreaded operation we really need to lock here and queue
     unrecognized packets... */
  while (pbuf = nidOsNextPacket(tport, fd, &len, wait)) {
    int r;
    r = nidOs_n2h_uint32(((NidHeader *)pbuf)->request);
    if (req == r || req == -1)
      break;
  }
  if (pbuf == NULL)
    return NULL;
  
  /* now decode */
  c = pbuf;
  pkt = malloc(sizeof(NidPacket));
  assert(pkt != NULL);
  pkt->header.size = nidOs_n2h_uint32(((NidHeader *)c)->size);
  pkt->header.request = nidOs_n2h_uint32(((NidHeader *)c)->request);
  pkt->header.type = nidOs_n2h_uint32(((NidHeader *)c)->type);
  pkt->payload = NULL;
  c += sizeof(NidHeader);

  /* now find the payload */
  switch(pkt->header.type) {
    /* general packets */
  case NID_PKT_HANDSHAKE:
    {
      UNPACK_PKTTYPE(NidHandshake);
      UNPACK_UINT32(out->protocol_major,c);
      UNPACK_UINT32(out->protocol_minor,c);
      break;
    }
  case NID_PKT_GET_VALUE:
  case NID_PKT_SET_VALUE:
  case NID_PKT_RETURN_VALUE:
    {
      UNPACK_PKTTYPE(NidValue);
      UNPACK_UINT32(out->id,c);
      UNPACK_INT32(out->value,c);
      break;
    }
  case NID_PKT_ACK:
    break;
  case NID_PKT_NAK:
    {
      UNPACK_PKTTYPE(NidNak);
      UNPACK_UINT32(out->reason,c);
      break;
    }
  case NID_PKT_TIME_SYNCH:
    {
      UNPACK_PKTTYPE(NidTimeSynch);
      UNPACK_UINT32(out->refpt,c);
      UNPACK_STR(out->absolute,c);
      break;
    }

    /* client requests */
  case NID_PKT_ENUM_DEVICES:
    break;

  case NID_PKT_ENUM_ELEMENTS:
    {
      UNPACK_PKTTYPE(NidEnumElements);
      UNPACK_UINT32(out->id,c);
      break;
    }

  case NID_PKT_QUERY_ELEMENTS:
  case NID_PKT_LISTEN_ELEMENTS:
  case NID_PKT_IGNORE_ELEMENTS:
    {
      int j;
      UNPACK_PKTTYPE(NidElementRequests);

      UNPACK_UINT32(out->num_requests,c)
      out->requests = malloc(sizeof(NidElementId)*out->num_requests);
      assert(out->requests != NULL);
      for(j = 0; j < out->num_requests; j++) {
	UNPACK_UINT32(out->requests[j].devid,c);
	UNPACK_UINT32(out->requests[j].elemid,c);
      }
      break;
    }

  case NID_PKT_FIND_DEVICE:
    {
      UNPACK_PKTTYPE(NidFindDevice);
      UNPACK_STR(out->name,c);
      UNPACK_STR(out->type,c);
      break;
    }

  case NID_PKT_SET_EVENT_SINK:
    {
      UNPACK_PKTTYPE(NidEventSink);
      UNPACK_SSTR(out->sink,c);
      break;
    }

    /* server responses */
  case NID_PKT_DEVICE_LIST:
    {
      int j;
      UNPACK_PKTTYPE(NidDeviceList);
      UNPACK_UINT32(out->num_devices,c);
      out->devices = malloc(sizeof(NidDeviceInfo)*out->num_devices);
      assert(out->devices != NULL);
      for(j = 0; j < out->num_devices; j++) {
	UNPACK_UINT32(out->devices[j].devid,c);
	UNPACK_STR(out->devices[j].name,c);
	UNPACK_STR(out->devices[j].type,c);
      }
      break;
    }
    
  case NID_PKT_ELEMENT_LIST:
    {
      int j,k;

      UNPACK_PKTTYPE(NidElementList);
      UNPACK_UINT32(out->devid,c);
      UNPACK_UINT32(out->num_elems,c);
      out->elements = malloc(sizeof(NidElementInfo)*out->num_elems);
      assert(out->elements != NULL);
      for(j = 0; j < out->num_elems; j++) {
	UNPACK_STR(out->elements[j].name,c);
	UNPACK_UINT32(out->elements[j].elemid,c);
	UNPACK_UINT32(out->elements[j].type,c);
	UNPACK_UINT32(out->elements[j].vsize,c);
	for(k = 0; k < NID_VECTOR_MAX_SZ; k++) {
	  UNPACK_REAL(out->elements[j].min[k],c);
	}
	for(k = 0; k < NID_VECTOR_MAX_SZ; k++) {
	  UNPACK_REAL(out->elements[j].max[k],c);
	}
      }
      break;
    }

  case NID_PKT_ELEMENT_STATES:
  case NID_PKT_ELEMENT_EVENTS:
    {
      int j,k;
      UNPACK_PKTTYPE(NidStateList);
      UNPACK_UINT32(out->num_states,c);
      out->states = calloc(out->num_states,sizeof(NidElementState));
      assert(out->states != NULL);
      for(j = 0; j < out->num_states; j++) {
	UNPACK_UINT32(out->states[j].timestamp,c);
	UNPACK_UINT32(out->states[j].devid,c);
	UNPACK_UINT32(out->states[j].elemid,c);
	UNPACK_UINT32(out->states[j].type,c);
	switch(out->states[j].type) {
	case NID_ELEM_TRIGGER:
	  break;
	case NID_ELEM_SWITCH:
	  UNPACK_UINT32(out->states[j].data.switch_,c);
	  break;
	case NID_ELEM_VALUATOR:
	  UNPACK_REAL(out->states[j].data.valuator,c);
	  break;
	case NID_ELEM_KEYBOARD:
	  UNPACK_UINT32(out->states[j].data.keyboard.code,c);
	  UNPACK_UINT32(out->states[j].data.keyboard.state,c);
	  break;
	case NID_ELEM_VECTOR:
	  UNPACK_UINT32(out->states[j].data.vector.size,c);
	  for(k = 0; k < out->states[j].data.vector.size; k++) {
	    UNPACK_REAL(out->states[j].data.vector.data[k],c);
	  }
	  break;
	default:
	  /* protocol error */
	  break;
	}
      }
    }
    break;

  case NID_PKT_DEVICE_FUNC:
    {
      UNPACK_PKTTYPE(NidDeviceFunc);
      UNPACK_UINT32(out->devid,c);
      UNPACK_STR(out->func,c);
      UNPACK_STR(out->args,c);
    }
    break;

  case NID_PKT_DEVICE_RESP:
    {
      UNPACK_PKTTYPE(NidDeviceResp);
      UNPACK_UINT32(out->code,c);
      UNPACK_STR(out->data,c);
    }
    break;

  }
  free(pbuf);
#ifdef DEBUG
  logpkt("recv",pkt);
#endif /* DEBUG */
  return pkt;
}

NidPacket *nidReceivePacket(int i, int req) {
  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);
  return nidReceiveRawPacket(NID_TPORT_TCP,connections[i].socket,req,1);
}

#define XMIT_SETUP(sz) \
      char *c; \
      while ((pbufsize - use) < sz) { \
	pbufsize *= 2; \
	pbuf = realloc(pbuf,pbufsize); \
	assert(pbuf != NULL); \
      } \
      c = &(pbuf[use]); \
      use += sz;

#define PACK_UINT32(c,src) { *(nid_uint32 *)(c) = nidOs_h2n_uint32(src); \
(c) += sizeof(nid_uint32); }
#define PACK_INT32(c,src) { *(nid_int32 *)(c) = nidOs_h2n_int32(src); \
(c) += sizeof(nid_int32); }
#define PACK_REAL(c,src) { *(nid_real *)(c) = nidOs_h2n_real(src); \
(c) += sizeof(nid_real); }
#define PACK_STR(c,src) { strncpy((c),(src),NID_DEVICE_STR_SZ); \
(c) += NID_DEVICE_STR_SZ; *((c)-1) = '\0'; }
#define PACK_SSTR(c,src) { strncpy((c),(src),NID_SINK_STR_SZ); \
(c) += NID_SINK_STR_SZ; *((c)-1) = '\0'; }

int nidTransmitPacket(int i, NidPacket *pkt) {
  static char *pbuf = NULL;
  static int pbufsize = 0;
  NidHeader *h;
  int use;

  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);

#ifdef DEBUG
  logpkt("xmit",pkt);
#endif /* DEBUG */
  
  if (pbuf == NULL) {
    pbufsize = 4096;
    pbuf = malloc(pbufsize);
    assert(pbuf != NULL);
  }
  use = 0;
  
  /* allocate space for the header, but don't fill it in yet */
  use = sizeof(NidHeader);
  
  /* stick in payload */
  switch(pkt->header.type) {
    /* general packets */
  case NID_PKT_HANDSHAKE:
    {
      NidHandshake *in = (NidHandshake *)(pkt->payload);
      XMIT_SETUP(sizeof(NidHandshake));
      PACK_UINT32(c,in->protocol_major);
      PACK_UINT32(c,in->protocol_minor);
      break;
    }

  case NID_PKT_SET_VALUE:
  case NID_PKT_GET_VALUE:
  case NID_PKT_RETURN_VALUE:
    {
      NidValue *in = (NidValue *)(pkt->payload);
      XMIT_SETUP(sizeof(NidValue));
      PACK_UINT32(c,in->id);
      PACK_INT32(c,in->value);
      break;
    }
  case NID_PKT_ACK:
    /* no payload */
    break;

  case NID_PKT_NAK:
    {
      NidNak *in = (NidNak *)(pkt->payload);
      XMIT_SETUP(sizeof(NidNak));
      PACK_UINT32(c,in->reason);
      break;
    }

  case NID_PKT_TIME_SYNCH:
    {
      NidTimeSynch *in = (NidTimeSynch *)(pkt->payload);
      XMIT_SETUP(sizeof(NidTimeSynch));
      PACK_UINT32(c,in->refpt);
      PACK_STR(c,in->absolute);
      break;
    }

  /* client requests */
  case NID_PKT_ENUM_DEVICES:
    /* no payload */
    break;

  case NID_PKT_ENUM_ELEMENTS:
    {
      NidEnumElements *in = (NidEnumElements *)(pkt->payload);
      XMIT_SETUP(sizeof(NidEnumElements));
      PACK_UINT32(c,in->id);
      break;
    }
  
  case NID_PKT_QUERY_ELEMENTS:
  case NID_PKT_LISTEN_ELEMENTS:
  case NID_PKT_IGNORE_ELEMENTS:
    {
      NidElementRequests *in = (NidElementRequests *)(pkt->payload);
      int j,sz = sizeof(nid_uint32) + in->num_requests*sizeof(NidElementId);
      XMIT_SETUP(sz);
      PACK_UINT32(c,in->num_requests);
      for(j = 0; j < in->num_requests; j++) {
	PACK_UINT32(c,in->requests[j].devid);
	PACK_UINT32(c,in->requests[j].elemid);
      }
      break;
    }

  case NID_PKT_FIND_DEVICE:
    {
      NidFindDevice *in = (NidFindDevice *)(pkt->payload);
      XMIT_SETUP(sizeof(NidFindDevice));
      PACK_STR(c,in->name);
      PACK_STR(c,in->type);
    }
    break;

  case NID_PKT_DEVICE_FUNC:
    {
      NidDeviceFunc *in = (NidDeviceFunc *)(pkt->payload);
      XMIT_SETUP(sizeof(NidDeviceFunc));
      PACK_UINT32(c,in->devid);
      PACK_STR(c,in->func);
      PACK_STR(c,in->args);
    }
    break;

  case NID_PKT_DEVICE_RESP:
    {
      NidDeviceResp *in = (NidDeviceResp *)(pkt->payload);
      XMIT_SETUP(sizeof(NidDeviceResp));
      PACK_UINT32(c,in->code);
      PACK_STR(c,in->data);
    }
    break;

  case NID_PKT_SET_EVENT_SINK:
    {
      NidEventSink *in = (NidEventSink *)(pkt->payload);
      XMIT_SETUP(sizeof(NidEventSink));
      PACK_SSTR(c,in->sink);
    }
    break;

  /* server responses */
  case NID_PKT_DEVICE_LIST:
    {
      NidDeviceList *in = (NidDeviceList *)(pkt->payload);
      int j, sz = sizeof(nid_uint32) + in->num_devices*sizeof(NidDeviceInfo);
      XMIT_SETUP(sz);
      PACK_UINT32(c,in->num_devices);
      for(j = 0; j < in->num_devices; j++) {
	PACK_UINT32(c,in->devices[j].devid);
	PACK_STR(c,in->devices[j].name);
	PACK_STR(c,in->devices[j].type);
      }
    }
    break;

  case NID_PKT_ELEMENT_LIST:
    {
      NidElementList *in = (NidElementList *)(pkt->payload);
      int j, k, sz = 2*sizeof(nid_uint32) + in->num_elems*sizeof(NidElementInfo);
      XMIT_SETUP(sz);
      PACK_UINT32(c,in->devid);
      PACK_UINT32(c,in->num_elems);
      for(j = 0; j < in->num_elems; j++) {
	PACK_STR(c,in->elements[j].name);
	PACK_UINT32(c,in->elements[j].elemid);
	PACK_UINT32(c,in->elements[j].type);
	PACK_UINT32(c,in->elements[j].vsize);
	for(k = 0; k < NID_VECTOR_MAX_SZ; k++) {
	  PACK_REAL(c,in->elements[j].min[k]);
	}
	for(k = 0; k < NID_VECTOR_MAX_SZ; k++) {
	  PACK_REAL(c,in->elements[j].max[k]);
	}
      }
      break;
    }
  
  case NID_PKT_ELEMENT_STATES:
  case NID_PKT_ELEMENT_EVENTS:
    {
      NidStateList *in = (NidStateList *)(pkt->payload);
      int j, k, sz = sizeof(nid_uint32) + in->num_states*sizeof(NidElementState);
      XMIT_SETUP(sz);
      PACK_UINT32(c,in->num_states);
      for(j = 0; j < in->num_states; j++) {
	PACK_UINT32(c,in->states[j].timestamp);
	PACK_UINT32(c,in->states[j].devid);
	PACK_UINT32(c,in->states[j].elemid);
	PACK_UINT32(c,in->states[j].type);
	switch(in->states[j].type) {
	case NID_ELEM_TRIGGER:
	  break;
	case NID_ELEM_SWITCH:
	  PACK_UINT32(c,in->states[j].data.switch_);
	  break;
	case NID_ELEM_VALUATOR:
	  PACK_REAL(c,in->states[j].data.valuator);
	  break;
	case NID_ELEM_KEYBOARD:
	  PACK_UINT32(c,in->states[j].data.keyboard.code);
	  PACK_UINT32(c,in->states[j].data.keyboard.state);
	  break;
	case NID_ELEM_VECTOR:
	  PACK_UINT32(c,in->states[j].data.vector.size);
	  for(k = 0; k < in->states[j].data.vector.size; k++) {
	    PACK_REAL(c,in->states[j].data.vector.data[k]);
	  }
	  break;
	default:
	  /* protocol error */
	  break;
	}
      }
    }
    break;
  }

  /* get this pointer again - address in pbuf may have changed due to 
     realloc() */
  h = (NidHeader *)pbuf; 
  /* doesn't include size field */
  h->size = nidOs_h2n_uint32(use - sizeof(nid_uint32)); 
  h->request = nidOs_h2n_uint32(pkt->header.request);
  h->type = nidOs_h2n_uint32(pkt->header.type);

  /* finally... */
  if (pkt->header.type == NID_PKT_ELEMENT_EVENTS)
    return nidOsWritePacket(connections[i].evtport,connections[i].evsocket,pbuf,use);
  else
    return nidOsWritePacket(NID_TPORT_TCP,connections[i].socket,pbuf,use);
}

static NidPacket *exchange(int i, int type, void *payload, int expect) {
  NidPacket p, *resp;
  
  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);

  p.header.size = 0;
  p.header.type = type;
  p.header.request = next_request(i);
  p.payload = payload;

  nidTransmitPacket(i,&p);
  resp = nidReceivePacket(i,p.header.request);
  if (!resp) {
    connections[i].last_err = -1;
    return NULL;
  }

  if (resp->header.type == NID_PKT_NAK) {
    connections[i].last_err = ((NidNak *)(resp->payload))->reason;
    nidFreePacket(resp);
    return NULL;
  } else if (resp->header.type == expect) {
    return resp;
  } else {
    connections[i].last_err = -1;
    _nidLogMsg( "protocol error - unexpected packet type: %d\n",
	    resp->header.type);
    nidFreePacket(resp);
    return NULL;
  }
}

/* the handshake is identical from both the client and server's 
   point of view */
static int handshake(int i) {
  NidHandshake hs, *hs_resp;
  NidPacket p, *resp;

  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);
  
  /* handshake */
  p.header.type = NID_PKT_HANDSHAKE;
  p.header.request = 0;
  hs.protocol_major = NID_PROTO_MAJOR;
  hs.protocol_minor = NID_PROTO_MINOR;
  p.payload = &hs;
  nidTransmitPacket(i,&p);

  resp = nidReceivePacket(i,0);
  if (resp == NULL) {
    _nidLogMsg( "handshake failed - connection closed\n");
    return -1;
  }
  if (resp->header.type != NID_PKT_HANDSHAKE) {
    _nidLogMsg( "protocol error - remote did not send handshake - got %d instead\n", resp->header.type);
    return -1;
  }

  /* is this protocol okay? */
  hs_resp = (NidHandshake *)(resp->payload);
  /* for now, we require that both major and minor revision match */
  if (hs_resp->protocol_major != NID_PROTO_MAJOR ||
      hs_resp->protocol_minor != NID_PROTO_MINOR) {
    /* nope */
    NidNak n;
    _nidLogMsg( "handshake rejected - remote is protocol %d.%d," 
	    "local is %d.%d\n", hs_resp->protocol_major,
	    hs_resp->protocol_minor, NID_PROTO_MAJOR, NID_PROTO_MINOR);
    n.reason = 0; /* no particular reason */
    p.header.type = NID_PKT_NAK;
    p.header.request = 0;
    p.payload = &n;
    nidTransmitPacket(i,&p);
    return -1;
  }

  /* yep */
  p.header.type = NID_PKT_ACK;
  p.header.request = 0;
  nidTransmitPacket(i,&p);

  /* read next packet which should be the ACK/NAK from the remote */
  resp = nidReceivePacket(i,0);
  if (resp == NULL) {
    _nidLogMsg( "handshake with failed - connection closed\n");
    return -1;
  }

  switch (resp->header.type) {
  case NID_PKT_ACK: /* all's well */
    break;
  case NID_PKT_NAK:
    _nidLogMsg( "handshake failed - protocol rejected by remote");
    return -1;
  default:
    _nidLogMsg( "protocol error - handshake expected ACK or NAK but got %d instead\n", resp->header.type);
    return -1;
  }
  
  /* handshake succeeded - other side has accepted our protocol */
  return 0;
}


/* main client functions */
int nidOpen(char *host, int port) {
  int sock, id;

  /* try to open socket first */
  if (port <= 0)
    port = NID_DEFAULT_PORT;
  if ((sock = nidOsOpenSocket(host,port)) < 0)
    return -1; /* failed to open connection */
  if ((id = nidRegister(sock)) < 0) {
    nidOsCloseSocket(sock);
    return -1;
  } else
    return id;
}

int nidRegister(int sock) {
  int i;

  i = alloc_conn();
  connections[i].socket = connections[i].evsocket = sock;
  connections[i].evtport = NID_TPORT_TCP;

  if (handshake(i)) { /* handshake with remote failed */
    free_conn(i);
    return -1; /* handshake failed */
  }

  return i;
}

void nidClose(int i) {
  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);

  nidOsCloseSocket(connections[i].socket);
  free_conn(i);
}

int nidLastError(int i) {
  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);
  return connections[i].last_err;
}

char *nidLastErrorMsg(int i) {
  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);
  /* always return something valid here */
  return (connections[i].last_msg ? connections[i].last_msg : "");
}

/* Note time TIME_SYNCH has no response */
void nidTimeSynch(int i, nid_uint32 refpt, char *absolute) {
  NidPacket p;
  NidTimeSynch ts;

  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);

  ts.refpt = refpt;
  strncpy(ts.absolute,absolute,NID_DEVICE_STR_SZ);
  ts.absolute[NID_DEVICE_STR_SZ-1] = '\0';

  p.header.size = 0;
  p.header.type = NID_PKT_TIME_SYNCH;
  p.header.request = next_request(i);
  p.payload = &ts;
  nidTransmitPacket(i,&p);
}

int nidCompressEvents(int i) {
  NidPacket *p;
  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);
  if (p = exchange(i,NID_PKT_COMPRESS_EVENTS,NULL,NID_PKT_ACK)) {
    nidFreePacket(p);
    return 0;
  }
  return -1;
}

int nidUncompressEvents(int i) {
  NidPacket *p;
  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);
  if (p = exchange(i,NID_PKT_UNCOMPRESS_EVENTS,NULL,NID_PKT_ACK)) {
    nidFreePacket(p);
    return 0;
  }
  return -1;
}

NidStateList *nidDumpEvents(int i) {
  NidPacket *p;
  NidStateList *sl = NULL;
  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);
  if (p = exchange(i,NID_PKT_DUMP_EVENTS,NULL,NID_PKT_ELEMENT_EVENTS)) {
    sl = (NidStateList *)(p->payload);
    p->payload = NULL;
    nidFreePacket(p);
  }
  return sl;
}

int nidSetEventSink(int i, char *sink) {
  NidPacket *p;
  NidEventSink s;
  if (strlen(sink) >= NID_SINK_STR_SZ-1) {
    _nidLogMsg("nidSetEventSink: sink string is too long");
    return -1; /* this isn't going to work - string is too long */
  }
  strcpy(s.sink,sink);
  if (p = exchange(i,NID_PKT_SET_EVENT_SINK,&s,NID_PKT_ACK)) {
    nidFreePacket(p);
    return 0;
  }
  return -1;
}

/* !!! To get nidChangeTransport right, I need to do some fiddly-bit
   work with fairly low-level interfaces to sockets.  I need to think
   about what the interfaces between this and the OS-dependent layer 
   should be. */
int nidChangeTransport(int i, char *tport) {
  int res;

  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);

  if (strcmp(tport,"default") == 0) {
    if (connections[i].socket != connections[i].evsocket) {
      if ((res = nidSetEventSink(i,"default")) == 0) {
	close(connections[i].evsocket);
	connections[i].evsocket = connections[i].socket;
	connections[i].evtport = NID_TPORT_TCP;
      }
      return res;
    } else 
      return 0;
  } else {
    int nfd;
    if ((nfd = nidOSReconnect(i,connections[i].socket,tport)) < 0) {
      _nidLogMsg("nidOSReconnect failed");
      return -1;
    }
    if (connections[i].evsocket != connections[i].socket)
      close(connections[i].evsocket);
    connections[i].evsocket = nfd;
    if (strcmp(tport,"udp") == 0)
      connections[i].evtport = NID_TPORT_UDP;
    else
      connections[i].evtport = NID_TPORT_TCP;
    return 0;
  }
}

/* processes a transport request on a server */
int nidProcessTransport(int i, char *tport) {
  int fd;
  if (strcmp(tport,"default") == 0) {
    if (connections[i].socket != connections[i].evsocket) {
      close(connections[i].evsocket);
      connections[i].evsocket = connections[i].socket;
      connections[i].evtport = NID_TPORT_TCP;
    }
    return 0;
  } else {
    if ((fd = nidOSHandleReconnect(tport)) < 0)
      return -1;
    if (connections[i].evsocket != connections[i].socket)
      close(connections[i].evsocket);
    connections[i].evsocket = fd;
    if (strncmp(tport,"udp",3) == 0)
      connections[i].evtport = NID_TPORT_UDP;
    else
      connections[i].evtport = NID_TPORT_TCP;
    return 0;
  }
}

NidDeviceList *nidEnumDevices(int i) {
  NidPacket *p;
  NidDeviceList *dl;

  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);

  if (!(p = exchange(i,NID_PKT_ENUM_DEVICES,NULL,NID_PKT_DEVICE_LIST)))
    return NULL;
  dl = (NidDeviceList *)(p->payload);
  p->payload = NULL;
  nidFreePacket(p);
  return dl;
}

NidElementList *nidEnumElements(int i, int dev) {
  NidPacket *p;
  NidEnumElements elem;
  NidElementList *el;
  
  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);

  elem.id = dev;

  if (!(p = exchange(i,NID_PKT_ENUM_ELEMENTS,&elem,NID_PKT_ELEMENT_LIST)))
    return NULL;

  el = (NidElementList *)(p->payload);
  p->payload = NULL;
  nidFreePacket(p);
  return el;
}

static NidPacket *do_query_pkt(int i, NidElementId *ids, int nids,
			       int type, int expect) {
  NidPacket p, *resp;
  NidElementRequests er;
  
  p.header.type = type;
  p.header.request = next_request(i);
  er.num_requests = nids;
  er.requests = ids;
  p.payload = &er;

  nidTransmitPacket(i,&p);
  resp = nidReceivePacket(i,p.header.request);
  if (!resp) {
    connections[i].last_err = -1;
    return NULL;
  }
  
  if (resp->header.type == NID_PKT_NAK) {
    connections[i].last_err = ((NidNak *)(resp->payload))->reason;
    nidFreePacket(resp);
    return NULL;
  } else if (resp->header.type == expect) {
    return resp;
  } else {
    connections[i].last_err = -1;
    _nidLogMsg( "protocol error - unexpected packet type: %d\n",
	    resp->header.type);
    nidFreePacket(resp);
    return NULL;
  }
}

NidStateList *nidQueryElements(int i, NidElementId *ids, int nids) {
  NidPacket *p;
  NidElementRequests er;
  NidStateList *sl;

  er.num_requests = nids;
  er.requests = ids;
  if (!(p = exchange(i,NID_PKT_QUERY_ELEMENTS,&er,NID_PKT_ELEMENT_STATES)))
    return NULL;
  sl = (NidStateList *)(p->payload);
  p->payload = NULL;
  nidFreePacket(p);
  return sl;
}

int nidSetValue(int i, nid_uint32 id, nid_int32 val) {
  NidPacket *p;
  NidValue v;
  
  v.id = id;
  v.value = val;
  if (!(p = exchange(i,NID_PKT_SET_VALUE,&v,NID_PKT_ACK)))
    return -1;
  nidFreePacket(p);
  return 0;
}

int nidGetValue(int i, nid_uint32 id, nid_int32 *val_ret) {
  NidPacket *p;
  NidValue v;
  
  v.id = id;
  v.value = 0; /* value is irrelevant, but set it to something known
		  anyways */
  if (!(p = exchange(i,NID_PKT_GET_VALUE,&v,NID_PKT_RETURN_VALUE)))
    return -1;
  if (val_ret)
    *val_ret = ((NidValue *)(p->payload))->value;
  return 0;
}

int nidListenElements(int i, NidElementId *ids, int nids) {
  NidPacket *p;
  NidElementRequests er;

  er.num_requests = nids;
  er.requests = ids;
  if (!(p = exchange(i,NID_PKT_LISTEN_ELEMENTS,&er,NID_PKT_ACK)))
    return -1;
  nidFreePacket(p);
  return 0;
}

int nidIgnoreElements(int i, NidElementId *ids, int nids) {
  NidPacket *p;
  NidElementRequests er;

  er.num_requests = nids;
  er.requests = ids;
  if (!(p = exchange(i,NID_PKT_IGNORE_ELEMENTS,&er,NID_PKT_ACK)))
    return -1;
  nidFreePacket(p);
  return 0;
}

NidStateList *nidNextEvents(int i, int wait) {
  /* get next chunk of pending events or block until some available */
  NidPacket *p;
  NidStateList *sl;
  int fd;

  assert(i >= 0 && i < MAXCONNS);
  assert(connections[i].in_use);

  /* events come in with request of 0 */
  while ((p = nidReceiveRawPacket(connections[i].evtport,
				  connections[i].evsocket,0,wait)) && 
	 p->header.type != NID_PKT_ELEMENT_EVENTS) {
    nidFreePacket(p);
  }

  if (!p)
    return NULL;
  
  sl = (NidStateList *)(p->payload);
  p->payload = NULL;
  nidFreePacket(p);
  return sl;
}

void nidFreeDeviceList(NidDeviceList *dl) {
  if (dl) {
    if (dl->devices)
      free(dl->devices);
    free(dl);
  }
}

void nidFreeElementList(NidElementList *el) {
  if (el) {
    if (el->elements)
      free(el->elements);
    free(el);
  }
}

void nidFreeStateList(NidStateList *sl) {
  if (sl) {
    if (sl->states)
      free(sl->states);
    free(sl);
  }
}

void nidFreeElementRequests(NidElementRequests *er) {
  if (er) {
    if (er->requests)
      free(er->requests);
    free(er);
  }
}

NidDeviceList *nidFindDevice(int i, char *name, char *type) {
  NidFindDevice fdev;
  NidDeviceList *dl;
  NidPacket *p;

  /* build packet */
  strncpy(fdev.name,name,NID_DEVICE_STR_SZ);
  fdev.name[NID_DEVICE_STR_SZ-1] = '\0';
  strncpy(fdev.type,type,NID_DEVICE_STR_SZ);
  fdev.type[NID_DEVICE_STR_SZ-1] = '\0';

  if (!(p = exchange(i,NID_PKT_FIND_DEVICE,&fdev,NID_PKT_DEVICE_LIST)))
    return NULL;
  dl = (NidDeviceList *)(p->payload);
  p->payload = NULL;
  nidFreePacket(p);
  return dl;
}
