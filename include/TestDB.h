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
 *  Contains all the Type Definitions and functions declarations
 *  for the CUnit test database maintenance.
 *
 *  Aug 2001      Initial implementation. (AK)
 *
 *  09/Aug/2001   Added Preprocessor conditionals for the file. (AK)
 *
 *  24/aug/2001   Made the linked list from SLL to DLL(doubly linked list). (AK)
 *
 *  31-Aug-2004   Restructured to eliminate global variables error_number,
 *                g_pTestRegistry; new interface, support for deprecated
 *                version 1 interface, moved error handling code to
 *                CUError.[ch], moved test run counts and _TestResult out
 *                of TestRegistry to TestRun.h. (JDS)
 *
 *  01-Sep-2004   Added jmp_buf to CU_Test. (JDS)
 *
 *  05-Sep-2004   Added internal test interface. (JDS)
 *
 *  15-Apr-2006   Removed constraint that suites/tests be uniquely named.
 *                Added ability to turn individual tests/suites on or off.
 *                Moved doxygen comments for public API here to header. (JDS)
 *
 *  16-Avr-2007   Added setup and teardown functions. (CJN)
 *
 */

/** @file
 *  Management functions for tests, suites, and the test registry.
 *  Unit testing in CUnit follows the common structure of unit
 *  tests aggregated in suites, which are themselves aggregated
 *  in a test registry.  This module provides functions and
 *  typedef's to support the creation, registration, and manipulation
 *  of test cases, suites, and the registry.
 */
/** @addtogroup Framework
 *  @{
 */

#ifndef CUNIT_TESTDB_H_SEEN
#define CUNIT_TESTDB_H_SEEN

#include <setjmp.h>   /* jmp_buf */

#include "CUnit.h"
#include "CUError.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=================================================================
 *  Typedefs and Data Structures
 *=================================================================*/

typedef int  (*CU_InitializeFunc)(void);  /**< Signature for suite initialization function. */
typedef int  (*CU_CleanupFunc)(void);     /**< Signature for suite cleanup function. */
typedef void (*CU_TestFunc)(void);        /**< Signature for a testing function in a test case. */
typedef void (*CU_SetUpFunc)(void);       /**< Signature for a test SetUp function. */
typedef void (*CU_TearDownFunc)(void);    /**< Signature for a test TearDown function. */

/*-----------------------------------------------------------------
 * CU_Test, CU_pTest
 *-----------------------------------------------------------------*/
/** CUnit test case data type.
 *  CU_Test is a double linked list of unit tests.  Each test has
 *  a name, a callable test function, and a flag for whether the
 *  test is active and thus executed during a  test run.  A test
 *  also holds links to the next and previous tests in the list,
 *  as well as a jmp_buf reference for use in implementing fatal
 *  assertions.<br /><br />
 *
 *  Generally, the linked list includes tests which are associated
 *  with each other in a CU_Suite.  As a result, tests are run in
 *  the order in which they are added to a suite (see CU_add_test()).
 *  <br /><br />
 *
 *  It is recommended that the name of each CU_Test in a suite have
 *  a unique name.  Otherwise, only the first-registered test having
 *  a given name will be accessible by that name.  There are no
 *  restrictions on the test function.  This means that the same
 *  function could, in principle, be called more than once from
 *  different tests.
 *
 *  @see CU_Suite
 *  @see CU_TestRegistry
 */
typedef struct CU_Test
{
  char*           pName;      /**< Test name. */
  CU_BOOL         fActive;    /**< Flag for whether test is executed during a run. */
  CU_TestFunc     pTestFunc;  /**< Pointer to the test function. */
  jmp_buf*        pJumpBuf;   /**< Jump buffer for setjmp/longjmp test abort mechanism. */

  struct CU_Test* pNext;      /**< Pointer to the next test in linked list. */
  struct CU_Test* pPrev;      /**< Pointer to the previous test in linked list. */

} CU_Test;
typedef CU_Test* CU_pTest;    /**< Pointer to a CUnit test case. */

/*-----------------------------------------------------------------
 *  CU_Suite, CU_pSuite
 *-----------------------------------------------------------------*/
/** CUnit suite data type.
 *  CU_Suite is a linked list of CU_Test containers.  Each suite has
 *  a name, a count of registered unit tests, and a flag for whether
 *  the suite is active during test runs. It also holds pointers to
 *  optional initialization and cleanup functions.  If non-NULL, these
 *  are called before and after running the suite's tests, respectively.
 *  In addition, the suite holds a pointer to the head of the linked
 *  list of associated CU_Test objects.  Finally, pointers to the next
 *  and previous suites in the linked list are maintained.<br /><br />
 *
 *  Generally, the linked list includes suites which are associated with
 *  each other in a CU_TestRegistry.  As a result, suites are run in the
 *  order in which they are registered (see CU_add_suite()).<br /><br />
 *
 *  It is recommended that name of each CU_Suite in a test registry have
 *  a unique name.  Otherwise, only the first-registered suite having a
 *  given name will be accessible by name.  There are no restrictions on
 *  the contained tests.  This means that the same CU_Test could, in
 *  principle, be run more than once fron different suites.
 *
 *  @see CU_Test
 *  @see CU_TestRegistry
 */
typedef struct CU_Suite
{
  char*             pName;            /**< Suite name. */
  CU_BOOL           fActive;          /**< Flag for whether suite is executed during a run. */
  CU_pTest          pTest;            /**< Pointer to the 1st test in the suite. */
  CU_InitializeFunc pInitializeFunc;  /**< Pointer to the suite initialization function. */
  CU_CleanupFunc    pCleanupFunc;     /**< Pointer to the suite cleanup function. */
  CU_SetUpFunc      pSetUpFunc;       /**< Pointer to the test SetUp function. */
  CU_TearDownFunc   pTearDownFunc;    /**< Pointer to the test TearDown function. */

  unsigned int      uiNumberOfTests;  /**< Number of tests in the suite. */
  struct CU_Suite*  pNext;            /**< Pointer to the next suite in linked list. */
  struct CU_Suite*  pPrev;            /**< Pointer to the previous suite in linked list. */

  unsigned int      uiNumberOfTestsFailed;  /**< Number of failed tests in the suite. */
  unsigned int      uiNumberOfTestsSuccess; /**< Number of success tests in the suite. */
} CU_Suite;
typedef CU_Suite* CU_pSuite;          /**< Pointer to a CUnit suite. */

/*-----------------------------------------------------------------
 *  CU_TestRegistry, CU_pTestRegistry
 *-----------------------------------------------------------------*/
/** CUnit test registry data type.
 *  CU_TestRegisty is the repository for suites containing unit tests.
 *  The test registry maintains a count of the number of CU_Suite
 *  objects contained in the registry, as well as a count of the total
 *  number of CU_Test objects associated with those suites.  It also
 *  holds a pointer to the head of the linked list of CU_Suite objects.
 *  <br /><br />
 *
 *  With this structure, the user will normally add suites implictly to
 *  the internal test registry using CU_add_suite(), and then add tests
 *  to each suite using CU_add_test().  Test runs are then initiated
 *  using one of the appropriate functions in TestRun.c via one of the
 *  user interfaces.<br /><br />
 *
 *  Automatic creation and destruction of the internal registry and its
 *  objects is available using CU_initialize_registry() and
 *  CU_cleanup_registry(), respectively.  For internal and testing
 *  purposes, the internal registry can be retrieved and assigned.
 *  Functions are also provided for creating and destroying independent
 *  registries.<br /><br />
 *
 *  Note that earlier versions of CUnit also contained a pointer to a
 *  linked list of CU_FailureRecord objects (termed _TestResults).
 *  This has been removed from theregistry and relocated to TestRun.c.
 *
 *  @see CU_Test
 *  @see CU_Suite
 *  @see CU_initialize_registry()
 *  @see CU_cleanup_registry()
 *  @see CU_get_registry()
 *  @see CU_set_registry()
 *  @see CU_create_new_registry()
 *  @see CU_destroy_existing_registry()
 */
typedef struct CU_TestRegistry
{
#ifdef USE_DEPRECATED_CUNIT_NAMES
  /** Union to support v1.1-1 member name. */
  union {
    unsigned int uiNumberOfSuites;  /**< Number of suites in the test registry. */
    unsigned int uiNumberOfGroups;  /**< Deprecated (version 1). @deprecated Use uiNumberOfSuites. */
  };
  unsigned int uiNumberOfTests;     /**< Number of tests in the test registry. */
  /** Union to support v1.1-1 member name. */
  union {
    CU_pSuite    pSuite;            /**< Pointer to the 1st suite in the test registry. */
    CU_pSuite    pGroup;            /**< Deprecated (version 1). @deprecated Use pSuite. */
  };
#else
  unsigned int uiNumberOfSuites;    /**< Number of registered suites in the registry. */
  unsigned int uiNumberOfTests;     /**< Total number of registered tests in the registry. */
  CU_pSuite    pSuite;              /**< Pointer to the 1st suite in the test registry. */
#endif
} CU_TestRegistry;
typedef CU_TestRegistry* CU_pTestRegistry;  /**< Pointer to a CUnit test registry. */

/*=================================================================
 *  Public interface functions
 *=================================================================*/

CU_EXPORT
CU_ErrorCode CU_initialize_registry(void);
/**<
 *  Initializes the framework test registry.
 *  Any existing registry is freed, including all stored suites
 *  and associated tests.  It is not necessary to explicitly call
 *  CU_cleanup_registry() before reinitializing the framework.
 *  The most recent stored test results are also cleared.<br /><br />
 *
 *  <B>This function must not be called during a test run (checked
 *  by assertion)</B>
 *
 *  @return  CUE_NOMEMORY if memory for the new registry cannot
 *           be allocated, CUE_SUCCESS otherwise.
 *  @see CU_cleanup_registry
 *  @see CU_get_registry
 *  @see CU_set_registry
 *  @see CU_registry_initialized
 */

CU_EXPORT
void CU_cleanup_registry(void);
/**<
 *  Clears the test registry.
 *  The active test registry is freed, including all stored suites
 *  and associated tests.  The most recent stored test results are
 *  also cleared.  After calling this function, CUnit suites cannot
 *  be added until CU_initialize_registry() or CU_set_registry() is
 *  called.  Further, any pointers to suites or test cases held by
 *  the user will be invalidated by calling this function.<br /><br />
 *
 *  This function may be called multiple times without generating
 *  an error condition.  However, <B>this function must not be
 *  called during a test run (checked by assertion)</B></P>.
 *
 *  @see CU_initialize_registry
 *  @see CU_get_registry
 *  @see CU_set_registry
 */

CU_EXPORT CU_BOOL CU_registry_initialized(void);
/**<
 *  Checks whether the test registry has been initialized.
 *
 *  @return  CU_TRUE if the registry has been initialized,
 *           CU_FALSE otherwise.
 *  @see CU_initialize_registry
 *  @see CU_cleanup_registry
 */

CU_EXPORT
CU_pSuite CU_add_suite(const char *strName,
                       CU_InitializeFunc pInit,
                       CU_CleanupFunc pClean);
/**<
 *  Creates a new test suite and adds it to the test registry.
 *  This function creates a new test suite having the specified
 *  name and initialization/cleanup functions and adds it to the
 *  test registry.  The new suite will be active and able to be
 *  executed during a test run.  The test registry must be
 *  initialized before calling this function (checked by assertion).
 *  pInit and pClean may be NULL, in which case no corresponding
 *  initialization of cleanup function will be called when the suite
 *  is run.  strName may be empty ("") but may not be NULL.<br /><br />
 *
 *  The return value is a pointer to the newly-created suite, or
 *  NULL if there was a problem with the suite creation or addition.
 *  An error code is also set for the framework. Note that if the
 *  name specified for the new suite is a duplicate, the suite will
 *  be created and added but the error code will be set to CUE_DUP_SUITE.
 *  The duplicate suite will not be accessible by name.<br /><br />
 *
 *  NOTE - the CU_pSuite pointer returned should NOT BE FREED BY
 *  THE USER.  The suite is freed by the CUnit system when
 *  CU_cleanup_registry() is called.  <b>This function must not
 *  be called during a test run (checked by assertion)</b>. <br /><br />
 *
 *  CU_add_suite() sets the following error codes:
 *  - CUE_SUCCESS if no errors occurred.
 *  - CUE_NOREGISTRY if the registry has not been initialized.
 *  - CUE_NO_SUITENAME if strName is NULL.
 *  - CUE_DUP_SUITE if a suite having strName is already registered.
 *  - CUE_NOMEMORY if a memory allocation failed.
 *
 *  @param strName Name for the new test suite (non-NULL).
 *  @param pInit   Initialization function to call before running suite.
 *  @param pClean  Cleanup function to call after running suite.
 *  @return A pointer to the newly-created suite (NULL if creation failed)
 */

CU_EXPORT
CU_pSuite CU_add_suite_with_setup_and_teardown(const char *strName,
                       CU_InitializeFunc pInit,
                       CU_CleanupFunc pClean,
                       CU_SetUpFunc pSetup,
                       CU_TearDownFunc pTear);
/**<
 *  The same as CU_add_suite but also adds setup and tear down callbacks for
 *  each test in this suite.
 *
 *  @param pSetup  SetUp function to call before running each test.
 *  @param pTear   TearDown function to call after running each test.
 */

CU_EXPORT
CU_pSuite CU_get_suite(const char* strName);
/**<
 *  Retrieves the suite having the specified name.
 *  Searches the active test registry and returns a pointer to the 1st
 *  suite found.  NULL is returned if no suite having the specified name
 *  is found.  In addition, the framework error state is set to CUE_NOREGISTRY
 *  if the registry is not initialized or to CUE_NO_SUITENAME if strName is NULL.
 *  If the return value is NULL and framework error state is CUE_SUCCESS, then
 *  the search simply failed to find the specified name.
 *  Use CU_get_suite_at_pos() to retrieve a suite by position rather than name.
 *
 *  @param strName The name of the suite to search for (non-NULL).
 *  @return Returns a pointer to the suite, or NULL if not found or an error occurred.
 *  @see CU_get_suite_at_pos()
 */

CU_EXPORT
CU_pTest CU_add_test(CU_pSuite pSuite, const char* strName, CU_TestFunc pTestFunc);
/**<
 *  This function creates a new test having the specified name
 *  and function, and adds it to the specified suite.  The new test
 *  is active and able to be executed during a test run.  At present,
 *  there is no mechanism for creating a test case independent of a
 *  suite.  Neither pSuite, strName, nor pTestFunc may be NULL.
 *
 *  The return value is a pointer to the newly-created test, or
 *  NULL if there was a problem with the test creation or addition.
 *  An error code is also set for the framework. Note that if the
 *  name specified for the new test is a duplicate within pSuite,
 *  the test will be created and added but the error code will be
 *  set to CUE_DUP_TEST.  The duplicate test will not be accessible
 *  by name.<br /><br />
 *
 *  NOTE - the CU_pTest pointer returned should NOT BE FREED BY
 *  THE USER.  The test is freed by the CUnit system when
 *  CU_cleanup_registry() is called.  <b>This function must not
 *  be called during a test run (checked by assertion)</b>. <br /><br />

 *  CU_add_test() sets the following error codes:
 *  - CUE_SUCCESS if no errors occurred.
 *  - CUE_NOREGISTRY if the registry has not been initialized.
 *  - CUE_NOSUITE if pSuite is NULL.
 *  - CUE_NO_TESTNAME if strName is NULL.
 *  - CUE_NOTEST if pTestFunc is NULL.
 *  - CUE_DUP_TEST if a test having strName is already registered to pSuite.
 *  - CUE_NOMEMORY if a memory allocation failed.<br /><br />
 *
 *  @param pSuite  Test suite to which to add new test (non-NULL).
 *  @param strName Name for the new test case (non-NULL).
 *  @param pTest   Function to call when running the test (non-NULL).
 *  @return A pointer to the newly-created test (NULL if creation failed)
 */

CU_EXPORT
CU_pTest CU_get_test(CU_pSuite pSuite, const char *strName);
/**<
 *  Retrieves the test having the specified name.
 *  Searches pSuite and returns a pointer to the 1st test found named strName.
 *  NULL is returned if no test having the specified name is found in pSuite.
 *  In addition, the framework error state is set as follows:
 *    - CUE_NOREGISTRY if the registry is not initialized
 *    - CUE_NOSUITE if pSuite is NULL
 *    - CUE_NO_TESTNAME if strName is NULL.
 *
 *  If the return value is NULL and framework error state is CUE_SUCCESS, then
 *  the search simply failed to find the specified name.  Use CU_get_test_at_pos()
 *  to retrieve a test by position rather than name.
 *
 *  @param pSuite  Pointer to the suite to search (non-NULL).
 *  @param strName The name of the test to search for (non-NULL).
 *  @return Returns a pointer to the test, or NULL if not found or an error occurred.
 *  @see CU_get_test_at_pos()
 */

#define CU_ADD_TEST(suite, test) (CU_add_test(suite, #test, (CU_TestFunc)test))
/**< Shortcut macro for adding a test to a suite. */

/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
/*  This section is based conceptually on code
 *  Copyright (C) 2004  Aurema Pty Ltd.
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
 *
 *  Derived from code contributed by K. Cheung and Aurema Pty Ltd. (thanks!)
 *    test_case_t, test_group_t, test_suite_t
 */

/**
 *  Test case parameters structure.
 *  This data type is provided to assist CUnit users manage collections of
 *  tests and suites.  It is intended to be used to build arrays of test case
 *  parameters that can be then be referred to in a CU_suite_info_t variable.
 */
typedef struct CU_TestInfo {
	const char  *pName;      /**< Test name. */
	CU_TestFunc pTestFunc;  /**< Test function. */
} CU_TestInfo;
typedef CU_TestInfo* CU_pTestInfo;  /**< Pointer to CU_TestInfo type. */

/**
 *  Suite parameters.
 *  This data type is provided to assist CUnit users manage collections of
 *  tests and suites.  It is intended to be used to build arrays of suite
 *  parameters that can be passed to a bulk registration function such as
 *  CU_register_suite() or CU_register_suites().
 */
typedef struct CU_SuiteInfo {
    const char       *pName;         /**< Suite name. */
    CU_InitializeFunc pInitFunc;     /**< Suite initialization function. */
    CU_CleanupFunc    pCleanupFunc;  /**< Suite cleanup function */
    CU_SetUpFunc      pSetUpFunc;    /**< Pointer to the test SetUp function. */
    CU_TearDownFunc   pTearDownFunc; /**< Pointer to the test TearDown function. */
    CU_TestInfo      *pTests;        /**< Test case array - must be NULL terminated. */
} CU_SuiteInfo;
typedef CU_SuiteInfo* CU_pSuiteInfo;  /**< Pointer to CU_SuiteInfo type. */

#define CU_TEST_INFO_NULL { NULL, NULL }
/**< NULL CU_test_info_t to terminate arrays of tests. */
#define CU_SUITE_INFO_NULL { NULL, NULL, NULL, NULL, NULL, NULL }
/**< NULL CU_suite_info_t to terminate arrays of suites. */


CU_EXPORT CU_ErrorCode CU_register_suites(CU_SuiteInfo suite_info[]);
/**<
 *  Registers the suites in a single CU_SuiteInfo array.
 *  Multiple arrays can be registered using CU_register_nsuites().
 *
 *  @param	suite_info NULL-terminated array of CU_SuiteInfo items to register.
 *  @return A CU_ErrorCode indicating the error status.
 *  @see CU_register_suites()
 */
CU_EXPORT CU_ErrorCode CU_register_nsuites(int suite_count, ...);
/**<
 *  Registers multiple suite arrays in CU_SuiteInfo format.
 *  The function accepts a variable number of suite arrays to be registered.
 *  The number of arrays is indicated by the value of the 1st argument,
 *  suite_count.  Each suite in each array is registered with the CUnit test
 *  registry, along with all of the associated tests.
 *
 *  @param	suite_count The number of CU_SuiteInfo* arguments to follow.
 *  @param ...          suite_count number of CU_SuiteInfo* arguments.  NULLs are ignored.
 *  @return A CU_ErrorCode indicating the error status.
 *  @see CU_register_suites()
 */

/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/

/*=================================================================
 *  Internal CUnit system functions.
 *  Should not be routinely called by users.
 *=================================================================*/

CU_EXPORT CU_pTestRegistry CU_get_registry(void);
/**<
 *  Retrieves a pointer to the current test registry.
 *  Returns NULL if the registry has not been initialized using
 *  CU_initialize_registry().  Directly accessing the registry
 *  should not be necessary for most users.  This function is
 *  provided primarily for internal and testing purposes.
 *
 *  @return A pointer to the current registry (NULL if uninitialized).
 *  @see CU_initialize_registry
 *  @see CU_set_registry
 */

CU_EXPORT CU_pTestRegistry CU_set_registry(CU_pTestRegistry pTestRegistry);
/**<
 *  Sets the registry to an existing CU_pTestRegistry instance.
 *  A pointer to the original registry is returned.  Note that the
 *  original registry is not freed, and it becomes the caller's
 *  responsibility to do so.  Directly accessing the registry
 *  should not be necessary for most users.  This function is
 *  provided primarily for internal and testing purposes.<br /><br />
 *
 *  <B>This function must not be called during a test run (checked
 *  by assertion)</B>.
 *
 *  @return A pointer to the original registry that was replaced.
 *  @see CU_initialize_registry
 *  @see CU_cleanup_registry
 *  @see CU_get_registry
 */

CU_EXPORT CU_pTestRegistry CU_create_new_registry(void);
/**<
 *  Creates and initializes a new test registry.
 *  Returns a pointer to a new, initialized registry (NULL if memory could
 *  not be allocated).  It is the caller's responsibility to destroy and free
 *  the new registry (unless it is made the active test registry using
 *  CU_set_registry()).
 */

CU_EXPORT
void CU_destroy_existing_registry(CU_pTestRegistry* ppRegistry);
/**<
 *  Destroys and frees all memory for an existing test registry.
 *  The active test registry is destroyed by the CUnit system in
 *  CU_cleanup_registry(), so only call this function on registries created
 *  or held independently of the internal CUnit system.<br /><br />
 *
 *  Once a registry is made the active test registry using CU_set_registry(),
 *  its destruction will be handled by the framework.  ppRegistry may not be
 *  NULL (checked by assertion), but *ppRegistry can be NULL (in which case the
 *  function has no effect).  Note that *ppRegistry will be set to NULL on return.
 *
 *  @param ppRegistry Address of a pointer to the registry to destroy (non-NULL).
 */

CU_EXPORT
CU_pTest CU_get_test_by_name(const char* szTestName, CU_pSuite pSuite);
/**<
 *  Retrieves a pointer to the test case in pSuite having the specified name.
 *  The first test case in pSuite having the specified name is returned, or
 *  NULL if not found.  Neither szSuiteName nor pSuite may be NULL (checked
 *  by assertion).  Clients should normally use CU_get_test() instead.
 *
 *  @param szTestName The name of the test case to locate (non-NULL).
 *  @param pSuite     The suite to scan (non-NULL).
 *  @return Pointer to the first test case having the specified name,
 *          NULL if not found.
 *  @see CU_get_test()
 */

#ifdef CUNIT_BUILD_TESTS
void test_cunit_TestDB(void);
#endif

#ifdef __cplusplus
}
#endif
#endif  /*  CUNIT_TESTDB_H_SEEN  */
/** @} */