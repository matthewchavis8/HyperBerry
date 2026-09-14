// @file cxxrt.h
// @brief Minimal C++ runtime support for the freestanding build.
// @ingroup lib
//
// Nothing runs global constructors on bare metal unless we do it ourselves.
// The compiler emits one initialiser per namespace-scope object into
// .init_array; the linker scripts bound that section, and hmain walks it.

#ifndef __CXXRT_H__
#define __CXXRT_H__

// @brief Run every global constructor collected into .init_array.
// @ingroup lib
//
// Call once, early in hmain, before anything touches a namespace-scope object.
// Order within the section is link order unless a constructor carries
// __attribute__((init_priority)), so a constructor must not depend on another
// translation unit's global already being built.
//
// @return Nothing.
void runGlobalConstructors();

#endif // !__CXXRT_H__
