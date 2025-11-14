#define QMEM_IMPLEMENTATION
#include "hunk.h"
#include<unistd.h>

int main() {
  H_InitHunk(1000);
  sleep(9999999);
  // H_MemInfo();
}
