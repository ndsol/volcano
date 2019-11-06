#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include "curl/system.h"

#define N (64)
int SIZEOF_INT[N];
short SIZEOF_SHORT[N];
long SIZEOF_LONG[N];
off_t SIZEOF_OFF_T[N];
curl_off_t SIZEOF_CURL_OFF_T[N];
size_t SIZEOF_SIZE_T[N];
time_t SIZEOF_TIME_T[N];

int main() {
  return 0;
}
