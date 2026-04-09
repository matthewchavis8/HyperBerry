#ifndef __DTB_H__
#define __DTB_H__

#include <stdint.h>
#include <stdbool.h>

struct alignas(16) MemoryMap {
  uint64_t memBase;  /**< Base physical address of the main RAM region.       */
  uint64_t memSize;  /**< Size in bytes of the main RAM region.               */
  uint64_t atfBase;  /**< Base physical address of the TF-A reserved region.  */
  uint64_t atfSize;  /**< Size in bytes of the TF-A reserved region.          */
  uint64_t dtbBase;  /**< Base physical address of the DTB blob.              */
  uint64_t dtbSize;  /**< Total size in bytes of the DTB blob.                */
  bool     isValid;    /**< True only if all fields were successfully parsed.   */
};

MemoryMap parseDtb(uintptr_t dtb);

#endif // __DTB_H__
