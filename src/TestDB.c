/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001            Anil Kumar
 *  Copyright (C) 2004,2005,2006  Anil Kumar, Jerry St.Clair
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
 *  Implementation of Registry/TestGroup/Testcase management Routines.
 *
 *  Aug 2001      Initial implementation (AK)
 *
 *  09/Aug/2001   Added startup initialize/cleanup registry functions. (AK)
 *
 *  29/Aug/2001   Added Test and Group Add functions. (AK)
 *
 *  02/Oct/2001   Added Proper Error codes and Messages on the failure conditions. (AK)
 *
 *  13/Oct/2001   Added Code to Check for the Duplicate Group name and test name. (AK)
 *
 *  15-Jul-2004   Added doxygen comments, new interface, added assertions to
 *                internal functions, moved error handling code to CUError.c,
 *                added assertions to make sure no modification of registry
 *                during a run, bug fixes, changed CU_set_registry() so that it
 *                doesn't require cleaning the existing registry. (JDS)
 *
 *  24-Apr-2006   Removed constraint that suites/tests be uniquely named.
 *                Added ability to turn individual tests/suites on or off.
 *                Added lookup functions for suites/tests based on index.
 *                Moved doxygen comments for public API here to header.
 *                Modified internal unit tests to include these changes.  (JDS)
 *
 *  02-May-2006   Added internationalization hooks.  (JDS)
 *
 *  16-Avr-2007   Added setup and teardown functions. (CJN)
 *
*/

/** @file
 *  Management functions for tests, suites, and the test registry (implementation).
 */
/** @addtogroup Framework
 @{
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

#include "CUnit.h"
#include "MyMem.h"
#include "TestDB.h"
#include "TestRun.h"
#include "Util.h"
#include "CUnit_intl.h"

/*=================================================================
 *  Global/Static Definitions
 *=================================================================*/
static CU_pTestRegistry f_pTestRegistry = NULL; /**< The active internal Test Registry. */


/* Instead of dynamic allocation static storage is used */

static struct {
  CU_Suite suite;
  char name[MAX_NAME_LEN];
} _suites_buf[MAX_NUM_OF_SUITES];

static struct {
  CU_Test test;
  char name[MAX_NAME_LEN];
} _tests_buf[MAX_NUM_OF_TESTS];

static struct {
    unsigned int currSuiteIndex;
    unsigned int currTestIndex;
} test_db_storage_info = { 0, 0 };

/*=================================================================
 * Private function forward declarations
 *=================================================================*/
static void      cleanup_test_registry(CU_pTestRegistry pRegistry);
static CU_pSuite create_suite(const char* strName, CU_InitializeFunc pInit, CU_CleanupFunc pClean, CU_SetUpFunc pSetup, CU_TearDownFunc pTear);
static void      cleanup_suite(CU_pSuite pSuite);
static void      insert_suite(CU_pTestRegistry pRegistry, CU_pSuite pSuite);
static CU_pTest  create_test(const char* strName, CU_TestFunc pTestFunc);
static void      cleanup_test(CU_pTest pTest);
static void      insert_test(CU_pSuite pSuite, CU_pTest pTest);

static CU_BOOL   suite_exists(CU_pTestRegistry pRegistry, const char* szSuiteName);
static CU_BOOL   test_exists(CU_pSuite pSuite, const char* szTestName);

static CU_Suite* getNewSuitePtr();
static CU_Test* getNewTestPtr();
/*=================================================================
 *  Public Interface functions
 *=================================================================*/
CU_ErrorCode CU_initialize_registry(void)
{
  CU_ErrorCode result;

  assert(CU_FALSE == CU_is_test_running());

  CU_set_error(result = CUE_SUCCESS);

  if (NULL != f_pTestRegistry) {
    CU_cleanup_registry();
  }

  f_pTestRegistry = CU_create_new_registry();
  if (NULL == f_pTestRegistry) {
    CU_set_error(result = CUE_NOMEMORY);
  }

  return result;
}

/*------------------------------------------------------------------------*/
CU_BOOL CU_registry_initialized(void)
{
  return (NULL == f_pTestRegistry) ? CU_FALSE : CU_TRUE;
}

/*------------------------------------------------------------------------*/
void CU_cleanup_registry(void)
{
  assert(CU_FALSE == CU_is_test_running());

  CU_set_error(CUE_SUCCESS);
  CU_destroy_existing_registry(&f_pTestRegistry);  /* supposed to handle NULL ok */
  CU_clear_previous_results();
  // CU_CREATE_MEMORY_REPORT(NULL);
}

/*------------------------------------------------------------------------*/
CU_pTestRegistry CU_get_registry(void)
{
  return f_pTestRegistry;
}

/*------------------------------------------------------------------------*/
CU_pTestRegistry CU_set_registry(CU_pTestRegistry pRegistry)
{
  CU_pTestRegistry pOldRegistry = f_pTestRegistry;

  assert(CU_FALSE == CU_is_test_running());

  CU_set_error(CUE_SUCCESS);
  f_pTestRegistry = pRegistry;
  return pOldRegistry;
}

/*------------------------------------------------------------------------*/
CU_pSuite CU_add_suite_with_setup_and_teardown(const char* strName, CU_InitializeFunc pInit, CU_CleanupFunc pClean, CU_SetUpFunc pSetup, CU_TearDownFunc pTear)
{
  CU_pSuite pRetValue = NULL;
  CU_ErrorCode error = CUE_SUCCESS;

  assert(CU_FALSE == CU_is_test_running());

  if (NULL == f_pTestRegistry) {
    error = CUE_NOREGISTRY;
  }
  else if (NULL == strName) {
    error = CUE_NO_SUITENAME;
  }
  else {
    pRetValue = create_suite(strName, pInit, pClean, pSetup, pTear);
    if (NULL == pRetValue) {
      error = CUE_NOMEMORY;
    }
    else {
      if (CU_TRUE == suite_exists(f_pTestRegistry, strName)) {
        error = CUE_DUP_SUITE;
      }
      insert_suite(f_pTestRegistry, pRetValue);
    }
  }

  CU_set_error(error);
  return pRetValue;
}

/*------------------------------------------------------------------------*/
CU_pSuite CU_add_suite(const char* strName, CU_InitializeFunc pInit, CU_CleanupFunc pClean)
{
  return CU_add_suite_with_setup_and_teardown(strName, pInit, pClean, NULL, NULL);
}

/*------------------------------------------------------------------------*/
CU_pTest CU_add_test(CU_pSuite pSuite, const char* strName, CU_TestFunc pTestFunc)
{
  CU_pTest pRetValue = NULL;
  CU_ErrorCode error = CUE_SUCCESS;

  assert(CU_FALSE == CU_is_test_running());

  if (NULL == f_pTestRegistry) {
    error = CUE_NOREGISTRY;
  }
  else if (NULL == pSuite) {
    error = CUE_NOSUITE;
  }
  else if (NULL == strName) {
    error = CUE_NO_TESTNAME;
  }
   else if(NULL == pTestFunc) {
    error = CUE_NOTEST;
  }
  else {
    pRetValue = create_test(strName, pTestFunc);
    if (NULL == pRetValue) {
      error = CUE_NOMEMORY;
    }
    else {
      f_pTestRegistry->uiNumberOfTests++;
      if (CU_TRUE == test_exists(pSuite, strName)) {
        error = CUE_DUP_TEST;
      }
      insert_test(pSuite, pRetValue);
    }
  }

  CU_set_error(error);
  return pRetValue;
}


/*=================================================================
 *  Private static function definitions
 *=================================================================*/
/*------------------------------------------------------------------------*/
/**
 *  Internal function to clean up the specified test registry.
 *  cleanup_suite() will be called for each registered suite to perform
 *  cleanup of the associated test cases.  Then, the suite's memory will
 *  be freed.  Note that any pointers to tests or suites in pRegistry
 *  held by the user will be invalidated by this function.  Severe problems
 *  can occur if this function is called during a test run involving pRegistry.
 *  Note that memory held for data members in the registry (e.g. pName) and
 *  the registry itself are not freed by this function.
 *
 *  @see cleanup_suite()
 *  @see cleanup_test()
 *  @param pRegistry CU_pTestRegistry to clean up (non-NULL).
 */
static void cleanup_test_registry(CU_pTestRegistry pRegistry)
{
  CU_pSuite pCurSuite = NULL;
  CU_pSuite pNextSuite = NULL;

  assert(NULL != pRegistry);

  pCurSuite = pRegistry->pSuite;
  while (NULL != pCurSuite) {
    pNextSuite = pCurSuite->pNext;
    cleanup_suite(pCurSuite);

    //CU_FREE(pCurSuite);
    pCurSuite = pNextSuite;
  }
  pRegistry->pSuite = NULL;
  pRegistry->uiNumberOfSuites = 0;
  pRegistry->uiNumberOfTests = 0;
}

/*------------------------------------------------------------------------*/
/**
 *  Internal function to create a new test suite having the specified parameters.
 *  This function creates a new test suite having the specified name and
 *  initialization/cleanup functions.  The new suite is active for execution during
 *  test runs.  The strName cannot be NULL (checked by assertion), but either or
 *  both function pointers can be.  A pointer to the newly-created suite is returned,
 *  or NULL if there was an error allocating memory for the new suite.  It is the
 *  responsibility of the caller to destroy the returned suite (use cleanup_suite()
 *  before freeing the returned pointer).
 *
 *  @param strName Name for the new test suite (non-NULL).
 *  @param pInit   Initialization function to call before running suite.
 *  @param pClean  Cleanup function to call after running suite.
 *  @return A pointer to the newly-created suite (NULL if creation failed)
 */
static CU_pSuite create_suite(const char* strName, CU_InitializeFunc pInit, CU_CleanupFunc pClean, CU_SetUpFunc pSetup, CU_TearDownFunc pTear)
{
  CU_pSuite pRetValue = getNewSuitePtr(); //(CU_pSuite)CU_ MALLOC(sizeof(CU_Suite));

  assert(NULL != strName);

  if (NULL != pRetValue) {
    // pRetValue->pName = (char *)CU_ MALLOC(strlen(strName)+1);
    if (NULL != pRetValue->pName) {
      strncpy(pRetValue->pName, strName, MAX_NAME_LEN);
      pRetValue->fActive = CU_TRUE;
      pRetValue->pInitializeFunc = pInit;
      pRetValue->pCleanupFunc = pClean;
      pRetValue->pSetUpFunc = pSetup;
      pRetValue->pTearDownFunc = pTear;
      pRetValue->pTest = NULL;
      pRetValue->pNext = NULL;
      pRetValue->pPrev = NULL;
      pRetValue->uiNumberOfTests = 0;
    }
    else {
      //CU_FREE(pRetValue);
      pRetValue = NULL;
    }
  }

  return pRetValue;
}

/*------------------------------------------------------------------------*/
/**
 *  Internal function to clean up the specified test suite.
 *  Each test case registered with pSuite will be freed.  Allocated memory held
 *  by the suite (i.e. the name) will also be deallocated.  Severe problems can
 *  occur if this function is called during a test run involving pSuite.
 *
 *  @param pSuite CU_pSuite to clean up (non-NULL).
 *  @see cleanup_test_registry()
 *  @see cleanup_test()
 */
static void cleanup_suite(CU_pSuite pSuite)
{
  CU_pTest pCurTest = NULL;
  CU_pTest pNextTest = NULL;

  assert(NULL != pSuite);

  pCurTest = pSuite->pTest;
  while (NULL != pCurTest) {
    pNextTest = pCurTest->pNext;

    cleanup_test(pCurTest);

    //CU_FREE(pCurTest);
    pCurTest = pNextTest;
  }
  if (NULL != pSuite->pName) {
    //CU_FREE(pSuite->pName);
  }

  pSuite->pName = NULL;
  pSuite->pTest = NULL;
  pSuite->uiNumberOfTests = 0;
}

/*------------------------------------------------------------------------*/
/**
 *  Internal function to insert a suite into a registry.
 *  The suite name is assumed to be unique.  Internally, the list of suites
 *  is a double-linked list, which this function manages.  Insertion of NULL
 *  pSuites is not allowed (checked by assertion).  Severe problems can occur
 *  if this function is called during a test run involving pRegistry.
 *
 *  @param pRegistry CU_pTestRegistry to insert into (non-NULL).
 *  @param pSuite    CU_pSuite to insert (non-NULL).
 *  @see insert_test()
 */
static void insert_suite(CU_pTestRegistry pRegistry, CU_pSuite pSuite)
{
  CU_pSuite pCurSuite = NULL;

  assert(NULL != pRegistry);
  assert(NULL != pSuite);

  pCurSuite = pRegistry->pSuite;

  assert(pCurSuite != pSuite);

  pSuite->pNext = NULL;
  pRegistry->uiNumberOfSuites++;

  /* if this is the 1st suite to be added... */
  if (NULL == pCurSuite) {
    pRegistry->pSuite = pSuite;
    pSuite->pPrev = NULL;
  }
  /* otherwise, add it to the end of the linked list... */
  else {
    while (NULL != pCurSuite->pNext) {
      pCurSuite = pCurSuite->pNext;
      assert(pCurSuite != pSuite);
    }

    pCurSuite->pNext = pSuite;
    pSuite->pPrev = pCurSuite;
  }
}

/*------------------------------------------------------------------------*/
/**
 *  Internal function to create a new test case having the specified parameters.
 *  This function creates a new test having the specified name and test function.
 *  The strName cannot be NULL (checked by assertion), but the function pointer
 *  may be.  A pointer to the newly-created test is returned, or NULL if there
 *  was an error allocating memory for the new test.  It is the responsibility
 *  of the caller to destroy the returned test (use cleanup_test() before freeing
 *  the returned pointer).
 *
 *  @param strName   Name for the new test.
 *  @param pTestFunc Test function to call when running this test.
 *  @return A pointer to the newly-created test (NULL if creation failed)
 */
static CU_pTest create_test(const char* strName, CU_TestFunc pTestFunc)
{
  CU_pTest pRetValue = getNewTestPtr(); //(CU_pTest)CU_ MALLOC(sizeof(CU_Test));

  assert(NULL != strName);

  if (NULL != pRetValue) {
    // pRetValue->pName = (char *)CU_ MALLOC(strlen(strName)+1);
    if (NULL != pRetValue->pName) {
      strncpy(pRetValue->pName, strName, MAX_NAME_LEN);
      pRetValue->fActive = CU_TRUE;
      pRetValue->pTestFunc = pTestFunc;
      pRetValue->pJumpBuf = NULL;
      pRetValue->pNext = NULL;
      pRetValue->pPrev = NULL;
    }
    else {
      //CU_FREE(pRetValue);
      pRetValue = NULL;
    }
  }

  return pRetValue;
}
/*------------------------------------------------------------------------*/
/**
 *  Internal function to clean up the specified test.
 *  All memory associated with the test will be freed.  Severe problems can
 *  occur if this function is called during a test run involving pTest.
 *
 *  @param pTest CU_pTest to clean up (non-NULL).
 *  @see cleanup_test_registry()
 *  @see cleanup_suite()
 */
static void cleanup_test(CU_pTest pTest)
{
  assert(NULL != pTest);

  if (NULL != pTest->pName) {
    // CU_FREE(pTest->pName);
  }

  pTest->pName = NULL;
}


/*------------------------------------------------------------------------*/
/**
 *  Internal function to insert a test into a suite.
 *  The test name is assumed to be unique.  Internally, the list of tests in
 *  a suite is a double-linked list, which this function manages.   Neither
 *  pSuite nor pTest may be NULL (checked by assertion).  Further, pTest must
 *  be an independent test (i.e. both pTest->pNext and pTest->pPrev == NULL),
 *  which is also checked by assertion.  Severe problems can occur if this
 *  function is called during a test run involving pSuite.
 *
 *  @param pSuite CU_pSuite to insert into (non-NULL).
 *  @param pTest  CU_pTest to insert (non-NULL).
 *  @see insert_suite()
 */
static void insert_test(CU_pSuite pSuite, CU_pTest pTest)
{
  CU_pTest pCurTest = NULL;

  assert(NULL != pSuite);
  assert(NULL != pTest);
  assert(NULL == pTest->pNext);
  assert(NULL == pTest->pPrev);

  pCurTest = pSuite->pTest;

  assert(pCurTest != pTest);

  pSuite->uiNumberOfTests++;
  /* if this is the 1st suite to be added... */
  if (NULL == pCurTest) {
    pSuite->pTest = pTest;
    pTest->pPrev = NULL;
  }
  else {
    while (NULL != pCurTest->pNext) {
      pCurTest = pCurTest->pNext;
      assert(pCurTest != pTest);
    }

    pCurTest->pNext = pTest;
    pTest->pPrev = pCurTest;
  }
}

/*------------------------------------------------------------------------*/
/**
 *  Internal function to check whether a suite having a specified
 *  name already exists.
 *
 *  @param pRegistry   CU_pTestRegistry to check (non-NULL).
 *  @param szSuiteName Suite name to check (non-NULL).
 *  @return CU_TRUE if suite exists in the registry, CU_FALSE otherwise.
 */
static CU_BOOL suite_exists(CU_pTestRegistry pRegistry, const char* szSuiteName)
{
  CU_pSuite pSuite = NULL;

  assert(NULL != pRegistry);
  assert(NULL != szSuiteName);

  pSuite = pRegistry->pSuite;
  while (NULL != pSuite) {
    if ((NULL != pSuite->pName) && (0 == CU_compare_strings(szSuiteName, pSuite->pName))) {
      return CU_TRUE;
    }
    pSuite = pSuite->pNext;
  }

  return CU_FALSE;
}

/*------------------------------------------------------------------------*/
/**
 *  Internal function to check whether a test having a specified
 *  name is already registered in a given suite.
 *
 *  @param pSuite     CU_pSuite to check (non-NULL).
 *  @param szTestName Test case name to check (non-NULL).
 *  @return CU_TRUE if test exists in the suite, CU_FALSE otherwise.
 */
static CU_BOOL test_exists(CU_pSuite pSuite, const char* szTestName)
{
  CU_pTest pTest = NULL;

  assert(NULL != pSuite);
  assert(NULL != szTestName);

  pTest = pSuite->pTest;
  while (NULL != pTest) {
    if ((NULL != pTest->pName) && (0 == CU_compare_strings(szTestName, pTest->pName))) {
      return CU_TRUE;
    }
    pTest = pTest->pNext;
  }

  return CU_FALSE;
}


/*------------------------------------------------------------------------*/
CU_pTest CU_get_test_by_name(const char* szTestName, CU_pSuite pSuite)
{
  CU_pTest pTest = NULL;
  CU_pTest pCur = NULL;

  assert(NULL != pSuite);
  assert(NULL != szTestName);

  pCur = pSuite->pTest;
  while (NULL != pCur) {
    if ((NULL != pCur->pName) && (0 == CU_compare_strings(pCur->pName, szTestName))) {
      pTest = pCur;
      break;
    }
    pCur = pCur->pNext;
  }

  return pTest;
}





static CU_pSuite getNewSuitePtr()
{
  if (test_db_storage_info.currSuiteIndex >= MAX_NUM_OF_SUITES) {
    return NULL;
  }

  CU_pSuite pSuite = &_suites_buf[test_db_storage_info.currSuiteIndex].suite;
  pSuite->pName = _suites_buf[test_db_storage_info.currSuiteIndex].name;
  test_db_storage_info.currSuiteIndex++;
  
  return pSuite;
}

static CU_pTest getNewTestPtr()
{
  if (test_db_storage_info.currTestIndex >= MAX_NUM_OF_TESTS) {
    return NULL;
  }
  CU_pTest pTest = &_tests_buf[test_db_storage_info.currTestIndex].test;
  pTest->pName = _tests_buf[test_db_storage_info.currTestIndex].name;
  test_db_storage_info.currTestIndex++;
  
  return pTest;
}





/*=================================================================
 *  Public but primarily internal function definitions
 *=================================================================*/
CU_pTestRegistry CU_create_new_registry(void)
{
  static CU_TestRegistry registry;
  CU_pTestRegistry pRegistry = &registry;
  if (NULL != pRegistry) {
    pRegistry->pSuite = NULL;
    pRegistry->uiNumberOfSuites = 0;
    pRegistry->uiNumberOfTests = 0;
  }

  return pRegistry;
}

/*------------------------------------------------------------------------*/
void CU_destroy_existing_registry(CU_pTestRegistry* ppRegistry)
{
  assert(NULL != ppRegistry);

  /* Note - CU_cleanup_registry counts on being able to pass NULL */

  if (NULL != *ppRegistry) {
    cleanup_test_registry(*ppRegistry);
  }
  //CU_FREE(*ppRegistry);
  *ppRegistry = NULL;
}


/** @} */
