#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nid.h>

void usage(char *argv0) {
  fprintf(stderr, "usage: %s host port\n", argv0);
  exit(1);
}

char *etype_to_string(int etype) {
  static char str[80];
  
  switch(etype) {
  case NID_ELEM_TRIGGER:  return "trigger";
  case NID_ELEM_SWITCH:   return "switch";
  case NID_ELEM_VALUATOR: return "valuator";
  case NID_ELEM_KEYBOARD: return "keyboard";
  case NID_ELEM_VECTOR:   return "vector";
  default:
    sprintf(str,"unknown(%d)",etype);
    return str;
  }
}

void print_estate(NidElementState *e) {
  int i;
  printf("\t%4d %4d %s", e->devid, e->elemid, etype_to_string(e->type));
  switch(e->type) {
  case NID_ELEM_TRIGGER:
    break;
  case NID_ELEM_SWITCH:
    printf(" state=%d", e->data.switch_ ? 1 : 0);
    break;
  case NID_ELEM_VALUATOR:
    printf(" value=%g", e->data.valuator);
    break;
  case NID_ELEM_KEYBOARD:
    printf(" code=%d state=%d", e->data.keyboard.code, 
	   e->data.keyboard.state ? 1 : 0);
    break;
  case NID_ELEM_VECTOR:
    printf(" data=(");
    for(i = 0; i < e->data.vector.size; i++)
      if (i)
	printf(",%g",e->data.vector.data[i]);
      else
	printf("%g",e->data.vector.data[i]);
    printf(")");
  }
  printf("\n");
}

void print_einfo(NidElementInfo *e) {
  int i;
  printf("\t%4d %s %s", e->elemid, e->name, etype_to_string(e->type));
  switch(e->type) {
  case NID_ELEM_VECTOR:
    printf(" vsize=%d (", e->vsize);
    for(i = 0; i < e->vsize; i++) {
      if (i)
	printf(",");
      printf("%g..%g", e->min[i], e->max[i]);
    }
    printf(")");
    break;
  case NID_ELEM_VALUATOR:
    printf(" (%g..%g)", e->min[0], e->max[0]);
    break;
  }
  printf("\n");
}

int main(int argc, char **argv) {
  int conn;
  int i,j;
  NidElementId *ids = NULL;
  int nids, maxids = 0;
  NidDeviceList *devices;
  NidElementList *elems;
  NidStateList *states;

  if (argc != 3)
    usage(argv[0]);

  if ((conn = nidOpen(argv[1],atoi(argv[2]))) < 0) {
    fprintf(stderr, "could not connect to server %s:%d\n",
	    argv[1], atoi(argv[2]));
    usage(argv[0]);
  }
  if (nidChangeTransport(conn,"udp"))
    fprintf(stderr,"failed to change transport");

  printf("Connected to NID server at %s:%d\n", argv[1], atoi(argv[2]));

  if (!(devices = nidEnumDevices(conn))) {
    fprintf(stderr, "could not get device list - %s\n",
	    nidLastErrorMsg(conn));
    exit(1);
  }

  /* dump devices */
  printf("Devices (and elements):\n");
  for(i = 0; i < devices->num_devices; i++) {
    printf("%4d %s %s\n", devices->devices[i].devid,
	   devices->devices[i].name, devices->devices[i].type);
    if (!(elems = nidEnumElements(conn,devices->devices[i].devid)))
      printf("could not enum elements - %s\n", nidLastErrorMsg(conn));
    else {
      if (elems->num_elems > maxids) {
	maxids = elems->num_elems;
	ids = realloc(ids,sizeof(NidElementId)*maxids);
	assert(ids != NULL);
      }
      nids = 0;
      printf("\tElements:\n");
      for(j = 0; j < elems->num_elems; j++) {
	print_einfo(&(elems->elements[j]));
	if (nids < maxids) {
	  ids[nids].devid = devices->devices[i].devid;
	  ids[nids].elemid = elems->elements[j].elemid;
	  nids++;
	}
      }
      /* get element state... */
      if (!(states = nidQueryElements(conn,ids,nids)))
	printf("could not query elements - %s\n", nidLastErrorMsg(conn));
      else {
	for(j = 0; j < states->num_states; j++)
	  print_estate(&(states->states[j]));
	nidFreeStateList(states);
      }
      nidListenElements(conn,ids,nids);
      nidFreeElementList(elems);
    }
  }

  while(states = nidNextEvents(conn,1)) {
    /* display states */
    printf("Event:\n");
    for(j = 0; j < states->num_states; j++)
      print_estate(&(states->states[j]));
    nidFreeStateList(states);
  }
  
  printf("connection closed by server: %s\n", nidLastErrorMsg(conn));
  nidClose(conn);
}
