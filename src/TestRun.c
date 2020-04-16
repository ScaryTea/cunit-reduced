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
 *  Implementation of Test Run Interface.
 *
 *  Aug 2001      Initial implementaion (AK)
 *
 *  19/Aug/2001   Added initial registry/Suite/test framework implementation. (AK)
 *
 *  24/Aug/2001   Changed Data structure from SLL to DLL for all linked lists. (AK)
 *
 *  25/Nov/2001   Added notification for Suite Initialization failure condition. (AK)
 *
 *  5-Aug-2004    New interface, doxygen comments, moved add_failure on suite
 *                initialization so called even if a callback is not registered,
 *                moved CU_assertImplementation into TestRun.c, consolidated
 *                all run summary info out of CU_TestRegistry into TestRun.c,
 *                revised counting and reporting of run stats to cleanly
 *                differentiate suite, test, and assertion failures. (JDS)
 *
 *  1-Sep-2004    Modified CU_assertImplementation() and run_single_test() for
 *                setjmp/longjmp mechanism of aborting test runs, add asserts in
 *                CU_assertImplementation() to trap use outside a registered
 *                test function during an active test run. (JDS)
 *
 *  22-Sep-2004   Initial implementation of internal unit tests, added nFailureRecords
 *                to CU_Run_Summary, added CU_get_n_failure_records(), removed
 *                requirement for registry to be initialized in order to run
 *                CU_run_suite() and CU_run_test(). (JDS)
 *
 *  30-Apr-2005   Added callback for suite cleanup function failure,
 *                updated unit tests. (JDS)
 *
 *  23-Apr-2006   Added testing for suite/test deactivation, changing functions.
 *                Moved doxygen comments for public functions into header.
 *                Added type marker to CU_FailureRecord.
 *                Added support for tracking inactive suites/tests. (JDS)
 *
 *  02-May-2006   Added internationalization hooks.  (JDS)
 *
 *  02-Jun-2006   Added support for elapsed time.  Added handlers for suite
 *                start and complete events.  Reworked test run routines to
 *                better support these features, suite/test activation. (JDS)
 *
 *  16-Avr-2007   Added setup and teardown functions. (CJN)
 *
 */

/** @file
 *  Test run management functions (implementation).
 */
/** @addtogroup Framework
 @{
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
#include <time.h>

#include "CUnit.h"
#include "MyMem.h"
#include "TestDB.h"
#include "TestRun.h"
#include "Util.h"
#include "CUnit_intl.h"

/*=================================================================
 *  Global/Static Definitions
 *=================================================================*/
static CU_BOOL   f_bTestIsRunning = CU_FALSE; /**< Flag for whether a test run is in progress */
static CU_pSuite f_pCurSuite = NULL;          /**< Pointer to the suite currently being run. */
static CU_pTest  f_pCurTest  = NULL;          /**< Pointer to the test currently being run. */

/** CU_RunSummary to hold results of each test run. */
static CU_RunSummary f_run_summary = {"", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/** CU_pFailureRecord to hold head of failure record list of each test run. */
static CU_pFailureRecord f_failure_list = NULL;

/** CU_pFailureRecord to hold head of failure record list of each test run. */
static CU_pFailureRecord f_last_failure = NULL;

/** Flag for whether inactive suites/tests are treated as failures. */
static CU_BOOL f_failure_on_inactive = CU_TRUE;

/** Variable for storage of start time for test run. */
static clock_t f_start_time;


/** Pointer to the function to be called before running a suite. */
static CU_SuiteStartMessageHandler          f_pSuiteStartMessageHandler = NULL;

/** Pointer to the function to be called before running a test. */
static CU_TestStartMessageHandler           f_pTestStartMessageHandler = NULL;

/** Pointer to the function to be called after running a test. */
static CU_TestCompleteMessageHandler        f_pTestCompleteMessageHandler = NULL;

/** Pointer to the function to be called after running a suite. */
static CU_SuiteCompleteMessageHandler       f_pSuiteCompleteMessageHandler = NULL;

/** Pointer to the function to be called when all tests have been run. */
static CU_AllTestsCompleteMessageHandler    f_pAllTestsCompleteMessageHandler = NULL;

/** Pointer to the function to be called if a suite initialization function returns an error. */
static CU_SuiteInitFailureMessageHandler    f_pSuiteInitFailureMessageHandler = NULL;

/** Pointer to the function to be called if a suite cleanup function returns an error. */
static CU_SuiteCleanupFailureMessageHandler f_pSuiteCleanupFailureMessageHandler = NULL;



/* Static storage for CU_FailureRecord */
static struct {
  CU_FailureRecord failureRecord;
  char strFileName[MAX_NAME_LEN];
  char strCondition[MAX_NAME_LEN];
} _failure_records_buf[MAX_NUM_OF_TESTS];

static struct {
    unsigned int currFailureIndex;
} test_run_storage_info = { 0 };


/*=================================================================
 * Private function forward declarations
 *=================================================================*/
static void         clear_previous_results(CU_pRunSummary pRunSummary, CU_pFailureRecord* ppFailure);
static void         cleanup_failure_list(CU_pFailureRecord* ppFailure);
static CU_ErrorCode run_single_suite(CU_pSuite pSuite, CU_pRunSummary pRunSummary);
static CU_ErrorCode run_single_test(CU_pTest pTest, CU_pRunSummary pRunSummary);
static void         add_failure(CU_pFailureRecord* ppFailure,
                                CU_pRunSummary pRunSummary,
                                CU_FailureType type,
                                unsigned int uiLineNumber,
                                const char *szCondition,
                                const char *szFileName,
                                CU_pSuite pSuite,
                                CU_pTest pTest);

static CU_pFailureRecord getNewFailureRecordPtr();      

/*=================================================================
 *  Public Interface functions
 *=================================================================*/
CU_BOOL CU_assertImplementation(CU_BOOL bValue,
                                unsigned int uiLine,
                                const char *strCondition,
                                const char *strFile,
                                const char *strFunction,
                                CU_BOOL bFatal)
{
  /* not used in current implementation - stop compiler warning */
  CU_UNREFERENCED_PARAMETER(strFunction);

  /* these should always be non-NULL (i.e. a test run is in progress) */
  assert(NULL != f_pCurSuite);
  assert(NULL != f_pCurTest);

  ++f_run_summary.nAsserts;
  if (CU_FALSE == bValue) {
    ++f_run_summary.nAssertsFailed;
    add_failure(&f_failure_list, &f_run_summary, CUF_AssertFailed,
                uiLine, strCondition, strFile, f_pCurSuite, f_pCurTest);

    if ((CU_TRUE == bFatal) && (NULL != f_pCurTest->pJumpBuf)) {
      longjmp(*(f_pCurTest->pJumpBuf), 1);
    }
  }

  return bValue;
}

/*------------------------------------------------------------------------*/
void CU_set_suite_start_handler(CU_SuiteStartMessageHandler pSuiteStartHandler)
{
  f_pSuiteStartMessageHandler = pSuiteStartHandler;
}

/*------------------------------------------------------------------------*/
void CU_set_test_start_handler(CU_TestStartMessageHandler pTestStartHandler)
{
  f_pTestStartMessageHandler = pTestStartHandler;
}

/*------------------------------------------------------------------------*/
void CU_set_test_complete_handler(CU_TestCompleteMessageHandler pTestCompleteHandler)
{
  f_pTestCompleteMessageHandler = pTestCompleteHandler;
}

/*------------------------------------------------------------------------*/
void CU_set_suite_complete_handler(CU_SuiteCompleteMessageHandler pSuiteCompleteHandler)
{
  f_pSuiteCompleteMessageHandler = pSuiteCompleteHandler;
}

/*------------------------------------------------------------------------*/
void CU_set_all_test_complete_handler(CU_AllTestsCompleteMessageHandler pAllTestsCompleteHandler)
{
  f_pAllTestsCompleteMessageHandler = pAllTestsCompleteHandler;
}

/*------------------------------------------------------------------------*/
void CU_set_suite_init_failure_handler(CU_SuiteInitFailureMessageHandler pSuiteInitFailureHandler)
{
  f_pSuiteInitFailureMessageHandler = pSuiteInitFailureHandler;
}

/*------------------------------------------------------------------------*/
void CU_set_suite_cleanup_failure_handler(CU_SuiteCleanupFailureMessageHandler pSuiteCleanupFailureHandler)
{
  f_pSuiteCleanupFailureMessageHandler = pSuiteCleanupFailureHandler;
}

/*------------------------------------------------------------------------*/
CU_SuiteStartMessageHandler CU_get_suite_start_handler(void)
{
  return f_pSuiteStartMessageHandler;
}

/*------------------------------------------------------------------------*/
CU_TestStartMessageHandler CU_get_test_start_handler(void)
{
  return f_pTestStartMessageHandler;
}

/*------------------------------------------------------------------------*/
CU_TestCompleteMessageHandler CU_get_test_complete_handler(void)
{
  return f_pTestCompleteMessageHandler;
}

/*------------------------------------------------------------------------*/
CU_SuiteCompleteMessageHandler CU_get_suite_complete_handler(void)
{
  return f_pSuiteCompleteMessageHandler;
}

/*------------------------------------------------------------------------*/
CU_AllTestsCompleteMessageHandler CU_get_all_test_complete_handler(void)
{
  return f_pAllTestsCompleteMessageHandler;
}

/*------------------------------------------------------------------------*/
CU_SuiteInitFailureMessageHandler CU_get_suite_init_failure_handler(void)
{
  return f_pSuiteInitFailureMessageHandler;
}

/*------------------------------------------------------------------------*/
CU_SuiteCleanupFailureMessageHandler CU_get_suite_cleanup_failure_handler(void)
{
  return f_pSuiteCleanupFailureMessageHandler;
}

/*------------------------------------------------------------------------*/
unsigned int CU_get_number_of_suites_run(void)
{
  return f_run_summary.nSuitesRun;
}

/*------------------------------------------------------------------------*/
unsigned int CU_get_number_of_suites_failed(void)
{
  return f_run_summary.nSuitesFailed;
}

/*------------------------------------------------------------------------*/
unsigned int CU_get_number_of_suites_inactive(void)
{
  return f_run_summary.nSuitesInactive;
}

/*------------------------------------------------------------------------*/
unsigned int CU_get_number_of_tests_run(void)
{
  return f_run_summary.nTestsRun;
}

/*------------------------------------------------------------------------*/
unsigned int CU_get_number_of_tests_failed(void)
{
  return f_run_summary.nTestsFailed;
}

/*------------------------------------------------------------------------*/
unsigned int CU_get_number_of_tests_inactive(void)
{
  return f_run_summary.nTestsInactive;
}

/*------------------------------------------------------------------------*/
unsigned int CU_get_number_of_asserts(void)
{
  return f_run_summary.nAsserts;
}

/*------------------------------------------------------------------------*/
unsigned int CU_get_number_of_successes(void)
{
  return (f_run_summary.nAsserts - f_run_summary.nAssertsFailed);
}

/*------------------------------------------------------------------------*/
unsigned int CU_get_number_of_failures(void)
{
  return f_run_summary.nAssertsFailed;
}

/*------------------------------------------------------------------------*/
unsigned int CU_get_number_of_failure_records(void)
{
  return f_run_summary.nFailureRecords;
}

/*------------------------------------------------------------------------*/
double CU_get_elapsed_time(void)
{
  if (CU_TRUE == f_bTestIsRunning) {
    return ((double)CU_get_time() - (double)f_start_time)/(double)CLOCKS_PER_SEC;
  }
  else {
    return f_run_summary.ElapsedTime;
  }
}

/*------------------------------------------------------------------------*/
CU_pFailureRecord CU_get_failure_list(void)
{
  return f_failure_list;
}

/*------------------------------------------------------------------------*/
CU_pRunSummary CU_get_run_summary(void)
{
  return &f_run_summary;
}

/*------------------------------------------------------------------------*/
CU_ErrorCode CU_run_all_tests(void)
{
  CU_pTestRegistry pRegistry = CU_get_registry();
  CU_pSuite pSuite = NULL;
  CU_ErrorCode result = CUE_SUCCESS;
  CU_ErrorCode result2;

  /* Clear results from the previous run */
  clear_previous_results(&f_run_summary, &f_failure_list);

  if (NULL == pRegistry) {
    result = CUE_NOREGISTRY;
  }
  else {
    /* test run is starting - set flag */
    f_bTestIsRunning = CU_TRUE;
    f_start_time = CU_get_time();

    pSuite = pRegistry->pSuite;
    while ((NULL != pSuite) && ((CUE_SUCCESS == result) || (CU_get_error_action() == CUEA_IGNORE))) {
      result2 = run_single_suite(pSuite, &f_run_summary);
      result = (CUE_SUCCESS == result) ? result2 : result;  /* result = 1st error encountered */
      pSuite = pSuite->pNext;
    }

    /* test run is complete - clear flag */
    f_bTestIsRunning = CU_FALSE;
    f_run_summary.ElapsedTime = ((double)CU_get_time() - (double)f_start_time)/(double)CLOCKS_PER_SEC;

    if (NULL != f_pAllTestsCompleteMessageHandler) {
     (*f_pAllTestsCompleteMessageHandler)(f_failure_list);
    }
  }

  CU_set_error(result);
  return result;
}

/*------------------------------------------------------------------------*/
CU_ErrorCode CU_run_suite(CU_pSuite pSuite)
{
  CU_ErrorCode result = CUE_SUCCESS;

  /* Clear results from the previous run */
  clear_previous_results(&f_run_summary, &f_failure_list);

  if (NULL == pSuite) {
    result = CUE_NOSUITE;
  }
  else {
    /* test run is starting - set flag */
    f_bTestIsRunning = CU_TRUE;
    f_start_time = CU_get_time();

    result = run_single_suite(pSuite, &f_run_summary);

    /* test run is complete - clear flag */
    f_bTestIsRunning = CU_FALSE;
    f_run_summary.ElapsedTime = ((double)CU_get_time() - (double)f_start_time)/(double)CLOCKS_PER_SEC;

    /* run handler for overall completion, if any */
    if (NULL != f_pAllTestsCompleteMessageHandler) {
      (*f_pAllTestsCompleteMessageHandler)(f_failure_list);
    }
  }

  CU_set_error(result);
  return result;
}

/*------------------------------------------------------------------------*/
CU_ErrorCode CU_run_test(CU_pSuite pSuite, CU_pTest pTest)
{
  CU_ErrorCode result = CUE_SUCCESS;
  CU_ErrorCode result2;

  /* Clear results from the previous run */
  clear_previous_results(&f_run_summary, &f_failure_list);

  if (NULL == pSuite) {
    result = CUE_NOSUITE;
  }
  else if (NULL == pTest) {
    result = CUE_NOTEST;
  }
  else if (CU_FALSE == pSuite->fActive) {
    f_run_summary.nSuitesInactive++;
    if (CU_FALSE != f_failure_on_inactive) {
      add_failure(&f_failure_list, &f_run_summary, CUF_SuiteInactive,
                  0, _("Suite inactive"), _("CUnit System"), pSuite, NULL);
    }
    result = CUE_SUITE_INACTIVE;
  }
  else if ((NULL == pTest->pName) || (NULL == CU_get_test_by_name(pTest->pName, pSuite))) {
    result = CUE_TEST_NOT_IN_SUITE;
  }
  else {
    /* test run is starting - set flag */
    f_bTestIsRunning = CU_TRUE;
    f_start_time = CU_get_time();

    f_pCurTest = NULL;
    f_pCurSuite = pSuite;

    pSuite->uiNumberOfTestsFailed = 0;
    pSuite->uiNumberOfTestsSuccess = 0;

    /* run handler for suite start, if any */
    if (NULL != f_pSuiteStartMessageHandler) {
      (*f_pSuiteStartMessageHandler)(pSuite);
    }

    /* run the suite initialization function, if any */
    if ((NULL != pSuite->pInitializeFunc) && (0 != (*pSuite->pInitializeFunc)())) {
      /* init function had an error - call handler, if any */
      if (NULL != f_pSuiteInitFailureMessageHandler) {
        (*f_pSuiteInitFailureMessageHandler)(pSuite);
      }
      f_run_summary.nSuitesFailed++;
      add_failure(&f_failure_list, &f_run_summary, CUF_SuiteInitFailed, 0,
                  _("Suite Initialization failed - Suite Skipped"),
                  _("CUnit System"), pSuite, NULL);
      result = CUE_SINIT_FAILED;
    }
    /* reach here if no suite initialization, or if it succeeded */
    else {
      result2 = run_single_test(pTest, &f_run_summary);
      result = (CUE_SUCCESS == result) ? result2 : result;

      /* run the suite cleanup function, if any */
      if ((NULL != pSuite->pCleanupFunc) && (0 != (*pSuite->pCleanupFunc)())) {
        /* cleanup function had an error - call handler, if any */
        if (NULL != f_pSuiteCleanupFailureMessageHandler) {
          (*f_pSuiteCleanupFailureMessageHandler)(pSuite);
        }
        f_run_summary.nSuitesFailed++;
        add_failure(&f_failure_list, &f_run_summary, CUF_SuiteCleanupFailed,
                    0, _("Suite cleanup failed."), _("CUnit System"), pSuite, NULL);
        result = (CUE_SUCCESS == result) ? CUE_SCLEAN_FAILED : result;
      }
    }

    /* run handler for suite completion, if any */
    if (NULL != f_pSuiteCompleteMessageHandler) {
      (*f_pSuiteCompleteMessageHandler)(pSuite, NULL);
    }

    /* test run is complete - clear flag */
    f_bTestIsRunning = CU_FALSE;
    f_run_summary.ElapsedTime = ((double)CU_get_time() - (double)f_start_time)/(double)CLOCKS_PER_SEC;

    /* run handler for overall completion, if any */
    if (NULL != f_pAllTestsCompleteMessageHandler) {
      (*f_pAllTestsCompleteMessageHandler)(f_failure_list);
    }

    f_pCurSuite = NULL;
  }

  CU_set_error(result);
  return result;
}

/*------------------------------------------------------------------------*/
void CU_clear_previous_results(void)
{
  clear_previous_results(&f_run_summary, &f_failure_list);
}

/*------------------------------------------------------------------------*/
CU_pSuite CU_get_current_suite(void)
{
  return f_pCurSuite;
}

/*------------------------------------------------------------------------*/
CU_pTest CU_get_current_test(void)
{
  return f_pCurTest;
}

/*------------------------------------------------------------------------*/
CU_BOOL CU_is_test_running(void)
{
  return f_bTestIsRunning;
}

/*------------------------------------------------------------------------*/
CU_EXPORT void CU_set_fail_on_inactive(CU_BOOL new_inactive)
{
  f_failure_on_inactive = new_inactive;
}

/*------------------------------------------------------------------------*/
CU_EXPORT CU_BOOL CU_get_fail_on_inactive(void)
{
  return f_failure_on_inactive;
}

/*------------------------------------------------------------------------*/
CU_EXPORT void CU_print_run_results(FILE *file)
{
  char *summary_string;

  assert(NULL != file);
  summary_string = CU_get_run_results_string();
  if (NULL != summary_string) {
    VLA_info("%s", summary_string);
    //CU_FREE(summary_string);
  }
  else {
    VLA_info(_("An error occurred printing the run results."));
  }
}

/*------------------------------------------------------------------------*/
CU_EXPORT char * CU_get_run_results_string(void)

{
#define BUF_SIZE 300
  static char buf[BUF_SIZE];

  CU_pRunSummary pRunSummary = &f_run_summary;
  CU_pTestRegistry pRegistry = CU_get_registry();
  int width[9];
  size_t len;
  char *result = buf;

  memset(result, '\0', BUF_SIZE);

  assert(NULL != pRunSummary);
  assert(NULL != pRegistry);

  width[0] = strlen(_("\nRun Summary:"));
  width[1] = CU_MAX(6,
                    CU_MAX(strlen(_("Type")),
                           CU_MAX(strlen(_("suites")),
                                  CU_MAX(strlen(_("tests")),
                                         strlen(_("asserts")))))) + 1;
  width[2] = CU_MAX(6,
                    CU_MAX(strlen(_("Total")),
                           CU_MAX(CU_number_width(pRegistry->uiNumberOfSuites),
                                  CU_MAX(CU_number_width(pRegistry->uiNumberOfTests),
                                         CU_number_width(pRunSummary->nAsserts))))) + 1;
  width[3] = CU_MAX(6,
                    CU_MAX(strlen(_("Ran")),
                           CU_MAX(CU_number_width(pRunSummary->nSuitesRun),
                                  CU_MAX(CU_number_width(pRunSummary->nTestsRun),
                                         CU_number_width(pRunSummary->nAsserts))))) + 1;
  width[4] = CU_MAX(6,
                    CU_MAX(strlen(_("Passed")),
                           CU_MAX(strlen(_("n/a")),
                                  CU_MAX(CU_number_width(pRunSummary->nTestsRun - pRunSummary->nTestsFailed),
                                         CU_number_width(pRunSummary->nAsserts - pRunSummary->nAssertsFailed))))) + 1;
  width[5] = CU_MAX(6,
                    CU_MAX(strlen(_("Failed")),
                           CU_MAX(CU_number_width(pRunSummary->nSuitesFailed),
                                  CU_MAX(CU_number_width(pRunSummary->nTestsFailed),
                                         CU_number_width(pRunSummary->nAssertsFailed))))) + 1;
  width[6] = CU_MAX(6,
                    CU_MAX(strlen(_("Inactive")),
                           CU_MAX(CU_number_width(pRunSummary->nSuitesInactive),
                                  CU_MAX(CU_number_width(pRunSummary->nTestsInactive),
                                         strlen(_("n/a")))))) + 1;

  width[7] = strlen(_("Elapsed time = "));
  width[8] = strlen(_(" seconds"));

  len = 13 + 4*(width[0] + width[1] + width[2] + width[3] + width[4] + width[5] + width[6]) + width[7] + width[8] + 1;
  // result = (char *)CU_ MALLOC(len);

  if (NULL != result) {
    snprintf(result, CU_MIN(BUF_SIZE, len), "%*s%*s%*s%*s%*s%*s%*s\n"   /* if you change this, be sure  */
                          "%*s%*s%*u%*u%*s%*u%*u\n"   /* to change the calculation of */
                          "%*s%*s%*u%*u%*u%*u%*u\n"   /* len above!                   */
                          "%*s%*s%*u%*u%*u%*u%*s\n\n",
                          //"%*s%8.3f%*s",
            width[0], _("\nRun Summary:"),
            width[1], _("Type"),
            width[2], _("Total"),
            width[3], _("Ran"),
            width[4], _("Passed"),
            width[5], _("Failed"),
            width[6], _("Inactive"),
            width[0], " ",
            width[1], _("suites"),
            width[2], pRegistry->uiNumberOfSuites,
            width[3], pRunSummary->nSuitesRun,
            width[4], _("n/a"),
            width[5], pRunSummary->nSuitesFailed,
            width[6], pRunSummary->nSuitesInactive,
            width[0], " ",
            width[1], _("tests"),
            width[2], pRegistry->uiNumberOfTests,
            width[3], pRunSummary->nTestsRun,
            width[4], pRunSummary->nTestsRun - pRunSummary->nTestsFailed,
            width[5], pRunSummary->nTestsFailed,
            width[6], pRunSummary->nTestsInactive,
            width[0], " ",
            width[1], _("asserts"),
            width[2], pRunSummary->nAsserts,
            width[3], pRunSummary->nAsserts,
            width[4], pRunSummary->nAsserts - pRunSummary->nAssertsFailed,
            width[5], pRunSummary->nAssertsFailed,
            width[6], _("n/a")); //,
            // width[7], _("Elapsed time = "), CU_get_elapsed_time(),  /* makes sure time is updated */
            // width[8], _(" seconds")
            // );
     result[len-1] = '\0';
  }
  return result;
}

/*=================================================================
 *  Static Function Definitions
 *=================================================================*/
/**
 *  Records a runtime failure.
 *  This function is called whenever a runtime failure occurs.
 *  This includes user assertion failures, suite initialization and
 *  cleanup failures, and inactive suites/tests when set as failures.
 *  This function records the details of the failure in a new
 *  failure record in the linked list of runtime failures.
 *
 *  @param ppFailure    Pointer to head of linked list of failure
 *                      records to append with new failure record.
 *                      If it points to a NULL pointer, it will be set
 *                      to point to the new failure record.
 *  @param pRunSummary  Pointer to CU_RunSummary keeping track of failure records
 *                      (ignored if NULL).
 *  @param type         Type of failure.
 *  @param uiLineNumber Line number of the failure, if applicable.
 *  @param szCondition  Description of failure condition
 *  @param szFileName   Name of file, if applicable
 *  @param pSuite       The suite being run at time of failure
 *  @param pTest        The test being run at time of failure
 */
static void add_failure(CU_pFailureRecord* ppFailure,
                        CU_pRunSummary pRunSummary,
                        CU_FailureType type,
                        unsigned int uiLineNumber,
                        const char *szCondition,
                        const char *szFileName,
                        CU_pSuite pSuite,
                        CU_pTest pTest)
{
  CU_pFailureRecord pFailureNew = NULL;
  CU_pFailureRecord pTemp = NULL;

  assert(NULL != ppFailure);

  pFailureNew = getNewFailureRecordPtr(); //(CU_pFailureRecord)CU_ MALLOC(sizeof(CU_FailureRecord));
  

  if (NULL == pFailureNew) {
    return;
  }

  // pFailureNew->strFileName = NULL;
  // pFailureNew->strCondition = NULL;
  if (NULL != szFileName) {
    // pFailureNew->strFileName = (char*)CU_ MALLOC(strlen(szFileName) + 1);
    if(NULL == pFailureNew->strFileName) {
      //CU_FREE(pFailureNew);
      return;
    }
    strncpy(pFailureNew->strFileName, szFileName, MAX_NAME_LEN);
  }

  if (NULL != szCondition) {
    // pFailureNew->strCondition = (char*)CU_ MALLOC(strlen(szCondition) + 1);
    if (NULL == pFailureNew->strCondition) {
      if(NULL != pFailureNew->strFileName) {
        //CU_FREE(pFailureNew->strFileName);
      }
      //CU_FREE(pFailureNew);
      return;
    }
    strncpy(pFailureNew->strCondition, szCondition, MAX_NAME_LEN);
  }

  pFailureNew->type = type;
  pFailureNew->uiLineNumber = uiLineNumber;
  pFailureNew->pTest = pTest;
  pFailureNew->pSuite = pSuite;
  pFailureNew->pNext = NULL;
  pFailureNew->pPrev = NULL;

  pTemp = *ppFailure;
  if (NULL != pTemp) {
    while (NULL != pTemp->pNext) {
      pTemp = pTemp->pNext;
    }
    pTemp->pNext = pFailureNew;
    pFailureNew->pPrev = pTemp;
  }
  else {
    *ppFailure = pFailureNew;
  }

  if (NULL != pRunSummary) {
    ++(pRunSummary->nFailureRecords);
  }
  
  f_last_failure = pFailureNew;
}




static CU_pFailureRecord getNewFailureRecordPtr()
{
  if (test_run_storage_info.currFailureIndex >= MAX_NUM_OF_TESTS) {
    return NULL;
  }

  CU_pFailureRecord pFailureRecord = &_failure_records_buf[test_run_storage_info.currFailureIndex].failureRecord;
  pFailureRecord->strFileName = _failure_records_buf[test_run_storage_info.currFailureIndex].strFileName;
  pFailureRecord->strCondition = _failure_records_buf[test_run_storage_info.currFailureIndex].strCondition;
  test_run_storage_info.currFailureIndex++;
  
  return pFailureRecord;
}






/*
 *  Local function for result set initialization/cleanup.
 */
/*------------------------------------------------------------------------*/
/**
 *  Initializes the run summary information in the specified structure.
 *  Resets the run counts to zero, and calls cleanup_failure_list() if
 *  failures were recorded by the last test run.  Calling this function
 *  multiple times, while inefficient, will not cause an error condition.
 *
 *  @param pRunSummary CU_RunSummary to initialize (non-NULL).
 *  @param ppFailure   The failure record to clean (non-NULL).
 *  @see CU_clear_previous_results()
 */
static void clear_previous_results(CU_pRunSummary pRunSummary, CU_pFailureRecord* ppFailure)
{
  assert(NULL != pRunSummary);
  assert(NULL != ppFailure);

  pRunSummary->nSuitesRun = 0;
  pRunSummary->nSuitesFailed = 0;
  pRunSummary->nSuitesInactive = 0;
  pRunSummary->nTestsRun = 0;
  pRunSummary->nTestsFailed = 0;
  pRunSummary->nTestsInactive = 0;
  pRunSummary->nAsserts = 0;
  pRunSummary->nAssertsFailed = 0;
  pRunSummary->nFailureRecords = 0;
  pRunSummary->ElapsedTime = 0.0;

  if (NULL != *ppFailure) {
    cleanup_failure_list(ppFailure);
  }

  f_last_failure = NULL;
}

/*------------------------------------------------------------------------*/
/**
 *  Frees all memory allocated for the linked list of test failure
 *  records.  pFailure is reset to NULL after its list is cleaned up.
 *
 *  @param ppFailure Pointer to head of linked list of
 *                   CU_pFailureRecords to clean.
 *  @see CU_clear_previous_results()
 */
static void cleanup_failure_list(CU_pFailureRecord* ppFailure)
{
  CU_pFailureRecord pCurFailure = NULL;
  CU_pFailureRecord pNextFailure = NULL;

  pCurFailure = *ppFailure;

  while (NULL != pCurFailure) {

    if (NULL != pCurFailure->strCondition) {
      //CU_FREE(pCurFailure->strCondition);
    }

    if (NULL != pCurFailure->strFileName) {
      //CU_FREE(pCurFailure->strFileName);
    }

    pNextFailure = pCurFailure->pNext;
    //CU_FREE(pCurFailure);
    pCurFailure = pNextFailure;
  }

  *ppFailure = NULL;
}

/*------------------------------------------------------------------------*/
/**
 *  Runs all tests in a specified suite.
 *  Internal function to run all tests in a suite.  The suite need
 *  not be registered in the test registry to be run.  Only
 *  suites having their fActive flags set CU_TRUE will actually be
 *  run.  If the CUnit framework is in an error condition after
 *  running a test, no additional tests are run.
 *
 *  @param pSuite The suite containing the test (non-NULL).
 *  @param pRunSummary The CU_RunSummary to receive the results (non-NULL).
 *  @return A CU_ErrorCode indicating the status of the run.
 *  @see CU_run_suite() for public interface function.
 *  @see CU_run_all_tests() for running all suites.
 */
static CU_ErrorCode run_single_suite(CU_pSuite pSuite, CU_pRunSummary pRunSummary)
{
  CU_pTest pTest = NULL;
  unsigned int nStartFailures;
  /* keep track of the last failure BEFORE running the test */
  CU_pFailureRecord pLastFailure = f_last_failure;
  CU_ErrorCode result = CUE_SUCCESS;
  CU_ErrorCode result2;

  assert(NULL != pSuite);
  assert(NULL != pRunSummary);

  nStartFailures = pRunSummary->nFailureRecords;

  f_pCurTest = NULL;
  f_pCurSuite = pSuite;

  /* run handler for suite start, if any */
  if (NULL != f_pSuiteStartMessageHandler) {
    (*f_pSuiteStartMessageHandler)(pSuite);
  }

  /* run suite if it's active */
  if (CU_FALSE != pSuite->fActive) {

    /* run the suite initialization function, if any */
    if ((NULL != pSuite->pInitializeFunc) && (0 != (*pSuite->pInitializeFunc)())) {
      /* init function had an error - call handler, if any */
      if (NULL != f_pSuiteInitFailureMessageHandler) {
        (*f_pSuiteInitFailureMessageHandler)(pSuite);
      }
      pRunSummary->nSuitesFailed++;
      add_failure(&f_failure_list, &f_run_summary, CUF_SuiteInitFailed, 0,
                  _("Suite Initialization failed - Suite Skipped"),
                  _("CUnit System"), pSuite, NULL);
      result = CUE_SINIT_FAILED;
    }

    /* reach here if no suite initialization, or if it succeeded */
    else {
      pTest = pSuite->pTest;
      while ((NULL != pTest) && ((CUE_SUCCESS == result) || (CU_get_error_action() == CUEA_IGNORE))) {
        if (CU_FALSE != pTest->fActive) {
          result2 = run_single_test(pTest, pRunSummary);
          result = (CUE_SUCCESS == result) ? result2 : result;
        }
        else {
          f_run_summary.nTestsInactive++;
          if (CU_FALSE != f_failure_on_inactive) {
            add_failure(&f_failure_list, &f_run_summary, CUF_TestInactive,
                        0, _("Test inactive"), _("CUnit System"), pSuite, pTest);
            result = CUE_TEST_INACTIVE;
          }
        }
        pTest = pTest->pNext;

        if (CUE_SUCCESS == result) {
          pSuite->uiNumberOfTestsFailed++;
        }
        else {
          pSuite->uiNumberOfTestsSuccess++;
        }
      }
      pRunSummary->nSuitesRun++;

      /* call the suite cleanup function, if any */
      if ((NULL != pSuite->pCleanupFunc) && (0 != (*pSuite->pCleanupFunc)())) {
        if (NULL != f_pSuiteCleanupFailureMessageHandler) {
          (*f_pSuiteCleanupFailureMessageHandler)(pSuite);
        }
        pRunSummary->nSuitesFailed++;
        add_failure(&f_failure_list, &f_run_summary, CUF_SuiteCleanupFailed,
                    0, _("Suite cleanup failed."), _("CUnit System"), pSuite, NULL);
        result = (CUE_SUCCESS == result) ? CUE_SCLEAN_FAILED : result;
      }
    }
  }

  /* otherwise record inactive suite and failure if appropriate */
  else {
    f_run_summary.nSuitesInactive++;
    if (CU_FALSE != f_failure_on_inactive) {
      add_failure(&f_failure_list, &f_run_summary, CUF_SuiteInactive,
                  0, _("Suite inactive"), _("CUnit System"), pSuite, NULL);
      result = CUE_SUITE_INACTIVE;
    }
  }

  /* if additional failures have occurred... */
  if (pRunSummary->nFailureRecords > nStartFailures) {
    if (NULL != pLastFailure) {
      pLastFailure = pLastFailure->pNext;  /* was a previous failure, so go to next one */
    }
    else {
      pLastFailure = f_failure_list;       /* no previous failure - go to 1st one */
    }
  }
  else {
    pLastFailure = NULL;                   /* no additional failure - set to NULL */
  }

  /* run handler for suite completion, if any */
  if (NULL != f_pSuiteCompleteMessageHandler) {
    (*f_pSuiteCompleteMessageHandler)(pSuite, pLastFailure);
  }

  f_pCurSuite = NULL;
  return result;
}

/*------------------------------------------------------------------------*/
/**
 *  Runs a specific test.
 *  Internal function to run a test case.  This includes calling
 *  any handler to be run before executing the test, running the
 *  test's function (if any), and calling any handler to be run
 *  after executing a test.  Suite initialization and cleanup functions
 *  are not called by this function.  A current suite must be set and
 *  active (checked by assertion).
 *
 *  @param pTest The test to be run (non-NULL).
 *  @param pRunSummary The CU_RunSummary to receive the results (non-NULL).
 *  @return A CU_ErrorCode indicating the status of the run.
 *  @see CU_run_test() for public interface function.
 *  @see CU_run_all_tests() for running all suites.
 */
static CU_ErrorCode run_single_test(CU_pTest pTest, CU_pRunSummary pRunSummary)
{
  volatile unsigned int nStartFailures;
  /* keep track of the last failure BEFORE running the test */
  volatile CU_pFailureRecord pLastFailure = f_last_failure;
  jmp_buf buf;
  CU_ErrorCode result = CUE_SUCCESS;

  assert(NULL != f_pCurSuite);
  assert(CU_FALSE != f_pCurSuite->fActive);
  assert(NULL != pTest);
  assert(NULL != pRunSummary);

  nStartFailures = pRunSummary->nFailureRecords;

  f_pCurTest = pTest;

  if (NULL != f_pTestStartMessageHandler) {
    (*f_pTestStartMessageHandler)(f_pCurTest, f_pCurSuite);
  }

  /* run test if it is active */
  if (CU_FALSE != pTest->fActive) {

    if (NULL != f_pCurSuite->pSetUpFunc) {
      (*f_pCurSuite->pSetUpFunc)();
    }

    /* set jmp_buf and run test */
    pTest->pJumpBuf = &buf;
    if (0 == setjmp(buf)) {
      if (NULL != pTest->pTestFunc) {
        (*pTest->pTestFunc)();
      }
    }

    if (NULL != f_pCurSuite->pTearDownFunc) {
       (*f_pCurSuite->pTearDownFunc)();
    }

    pRunSummary->nTestsRun++;
  }
  else {
    f_run_summary.nTestsInactive++;
    if (CU_FALSE != f_failure_on_inactive) {
      add_failure(&f_failure_list, &f_run_summary, CUF_TestInactive,
                  0, _("Test inactive"), _("CUnit System"), f_pCurSuite, f_pCurTest);
    }
    result = CUE_TEST_INACTIVE;
  }

  /* if additional failures have occurred... */
  if (pRunSummary->nFailureRecords > nStartFailures) {
    pRunSummary->nTestsFailed++;
    if (NULL != pLastFailure) {
      pLastFailure = pLastFailure->pNext;  /* was a previous failure, so go to next one */
    }
    else {
      pLastFailure = f_failure_list;       /* no previous failure - go to 1st one */
    }
  }
  else {
    pLastFailure = NULL;                   /* no additional failure - set to NULL */
  }

  if (NULL != f_pTestCompleteMessageHandler) {
    (*f_pTestCompleteMessageHandler)(f_pCurTest, f_pCurSuite, pLastFailure);
  }

  pTest->pJumpBuf = NULL;
  f_pCurTest = NULL;

  return result;
}

/** @} */
