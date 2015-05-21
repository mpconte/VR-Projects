#include <ve_version.h>

int veGetVersionMajor(void) {
  return VE_VERSION_MAJOR;
}

int veGetVersionMinor(void) {
  return VE_VERSION_MINOR;
}

int veGetVersionPatch(void) {
  return VE_VERSION_PATCH;
}

char *veGetVersionString(void) {
  return VE_VERSION_STRING;
}
