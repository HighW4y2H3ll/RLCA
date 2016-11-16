
#ifndef _RUTILS
#define _RUTILS

#include <stdlib.h>
#include "types.h"

// Sort Utils
void sortSlot ( void );
void sortRangeByAddr ( void );
void sortRangeBySz ( void );

// Search Utils
size_t searchRangeByAddr ( size_t start );
size_t searchRangeBySz ( size_t size );

#endif
