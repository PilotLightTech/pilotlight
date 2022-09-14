/*
   pl_os.h
     * platform services
     * no dependencies
     * simple
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] public api
*/

#ifndef PL_OS_H
#define PL_OS_H

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#include "pl.h"

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// files
void pl_read_file(const char* file, unsigned* size, char* buffer, const char* mode);

// misc
int pl_sleep(uint32_t millisec);

#endif // PL_OS_H