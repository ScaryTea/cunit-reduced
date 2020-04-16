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
 *  Generic (internal) utility functions used across CUnit.
 *  These were originally distributed across the other CUnit
 *  source files, but were consolidated here for consistency.
 *
 *  13/Oct/2001   Initial implementation (AK)
 *
 *  26/Jul/2003   Added a function to convert a string containing special
 *                characters into escaped character for XML/HTML usage. (AK)
 *
 *  16-Jul-2004   New interface, doxygen comments. (JDS)
 *
 *  17-Apr-2006   Added CU_translated_strlen() and CU_number_width().
 *                Fixed off-by-1 error in CU_translate_special_characters(),
 *                modifying implementation & results in some cases.  User can
 *                now tell if conversion failed. (JDS)
 */

/** @file
 *  Utility functions (implementation).
 */
/** @addtogroup Framework
 @{
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "CUnit.h"
#include "TestDB.h"
#include "Util.h"


/*------------------------------------------------------------------------*/
int CU_compare_strings(const char* szSrc, const char* szDest)
{
  assert(NULL != szSrc);
  assert(NULL != szDest);

	while (('\0' != *szSrc) && ('\0' != *szDest) && (toupper(*szSrc) == toupper(*szDest))) {
		szSrc++;
		szDest++;
	}

	return (int)(*szSrc - *szDest);
}

/*------------------------------------------------------------------------*/
size_t CU_number_width(int number)
{
	char buf[33];

	snprintf(buf, 33, "%d", number);
	buf[32] = '\0';
	return (strlen(buf));
}


/** @} */
