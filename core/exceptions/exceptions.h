/**
 * @file exceptions.h
 * @brief Exception context structure shared between assembly and C++.
 * @ingroup exceptions
 */

#ifndef __EXCEPTIONS_H__
#define __EXCEPTIONS_H__

#include <stdint.h>
#include "lib/array/array.h"

// ESR_EL2.EC — exception class field [31:26]
enum class EsrEc : uint64_t {
  Unknown        = 0x00,
  WfxTrap        = 0x01,
  McrMrc         = 0x03,
  McrrcMrrc      = 0x04,
  Cp15McrMrc     = 0x05,
  Cp15McrrcMrrc  = 0x06,
  Cp14McrMrc     = 0x07,
  FpAccess       = 0x07,
  Cp14Mrrc       = 0x0C,
  BranchTarget   = 0x0D,
  IllegalState   = 0x0E,
  SvcAarch32     = 0x11,
  HvcAarch32     = 0x12,
  SmcAarch32     = 0x13,
  SvcAarch64     = 0x15,
  HvcAarch64     = 0x16,
  SmcAarch64     = 0x17,
  MsrMrsTrap     = 0x18,
  SveAccess      = 0x19,
  PacTrap        = 0x1C,
  InstrAbortLower = 0x20,
  InstrAbortSame  = 0x21,
  PcAlignFault    = 0x22,
  DataAbortLower  = 0x24,
  DataAbortSame   = 0x25,
  SpAlignFault    = 0x26,
  FpExcAarch32    = 0x28,
  FpExcAarch64    = 0x2C,
  SError          = 0x2F,
  BreakpointLower = 0x30,
  BreakpointSame  = 0x31,
  SoftStepLower   = 0x32,
  SoftStepSame    = 0x33,
  WatchpointLower = 0x34,
  WatchpointSame  = 0x35,
  BkptAarch32     = 0x38,
  VectorCatch     = 0x3A,
  BrkAarch64      = 0x3C,
};

using ExceptionContext = hv::array<uint64_t, 31>;

inline EsrEc getEsrEc(uint64_t esr) {
  return static_cast<EsrEc>((esr >> 26) & 0x3F);
}

#endif // __EXCEPTIONS_H__
