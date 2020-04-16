/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001       Anil Kumar
 *  Copyright (C) 2004-2006  Anil Kumar, Jerry St.Clair
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *  Contains Type Definitions for some generic functions used across
 *  CUnit project files.
 *
 *  13/Oct/2001   Moved some of the generic functions declarations from
 *                other files to this one so as to use the functions
 *                consitently. This file is not included in the distribution
 *                headers because it is used internally by CUnit. (AK)
 *
 *  20-Jul-2004   New interface, support for deprecated version 1 names. (JDS)
 *
 *  5-Sep-2004    Added internal test interface. (JDS)
 *
 *  17-Apr-2006   Added CU_translated_strlen() and CU_number_width().
 *                Removed CUNIT_MAX_STRING_LENGTH - dangerous since not enforced.
 *                Fixed off-by-1 error in CU_translate_special_characters(),
 *                modifying implementation & results in some cases.  User can
 *                now tell if conversion failed. (JDS)
 */

/** @file
 *  Utility functions (user interface).
 */
/** @addtogroup Framework
 * @{
 */

#ifndef CUNIT_UTIL_H_SEEN
#define CUNIT_UTIL_H_SEEN

#include "CUnit.h"
#ifdef THREADX
#include "txm_module.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define CUNIT_MAX_ENTITY_LEN 5
/**< Maximum number of characters in a translated xml entity. */


CU_EXPORT int CU_compare_strings(const char *szSrc, const char *szDest);
/**<
 *  Case-insensitive string comparison.  Neither string pointer
 *  can be NULL (checked by assertion).
 *
 *  @param szSrc  1st string to compare (non-NULL).
 *  @param szDest 2nd string to compare (non-NULL).
 *  @return  0 if the strings are equal, non-zero otherwise.
 */


CU_EXPORT size_t CU_number_width(int number);
/**<
 *  Calulates the number of places required to display
 *  number in decimal.
 */

#ifdef LINUX
	#define CU_get_time()   clock()
#else
    #define CU_get_time()   (0xFFFFFFFFUL - tx_time_get())
#endif

/**<
 *  Get the relative time (in ticks). The actual time each 
 *  timer-tick represents is application specific.
 */

#ifdef __cplusplus
}
#endif

#endif /* CUNIT_UTIL_H_SEEN */
/** @} */
