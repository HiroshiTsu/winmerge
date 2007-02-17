/////////////////////////////////////////////////////////////////////////////
//    License (GPLv2+):
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or (at
//    your option) any later version.
//    
//    This program is distributed in the hope that it will be useful, but
//    WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  DiffThread.cpp
 *
 * @brief Code for DiffThread class
 */
// RCS ID line follows -- this is updated by CVS
// $Id$

#include "stdafx.h"
#include "diffcontext.h"
#include "diffthread.h"
#include "DirScan.h"
#include "Plugins.h"
#include "DiffItemList.h"
#include "PathContext.h"
#include "CompareStats.h"
#include "IAbortable.h"

// Set this to true in order to single step
// through entire compare process all in a single thread
// Either edit this line, or breakpoint & change it in CompareDirectories() below
static bool bSinglethreaded=false;

/**
 * @brief Structure used in sending data to the threads.
 * As thread functions have only one parameter we must pack all
 * the data we need inside structure.
 */
struct DiffFuncStruct
{
	CString path1;
	CString path2;
	CDiffContext * context;
	UINT msgUIUpdate;
	HWND hWindow;
	UINT nThreadState;
	BOOL bRecursive;
	DiffThreadAbortable * m_pAbortgate;
	bool bOnlyRequested;
	DiffItemList *pItemList;

	DiffFuncStruct()
		: context(NULL)
		, msgUIUpdate(0)
		, hWindow(0)
		, nThreadState(THREAD_NOTSTARTED)
		, bRecursive(FALSE)
		, m_pAbortgate(NULL)
		, bOnlyRequested(false)
		, pItemList(NULL)
		{}
};


/** @brief abort handler for CDiffThread -- just a gateway to CDiffThread */
class DiffThreadAbortable : public IAbortable
{
// Implement DirScan's IAbortable
public:
	virtual bool ShouldAbort() const { return m_diffthread->ShouldAbort(); }

// All this object does is forward ShouldAbort calls to its containing CDiffThread

	DiffThreadAbortable(CDiffThread * diffthread) : m_diffthread(diffthread) { }
	CDiffThread * m_diffthread;
};

/**
 * @brief Default constructor
 */
CDiffThread::CDiffThread()
: m_pDiffContext(NULL)
, m_msgUpdateUI(0)
, m_hWnd(0)
, m_bAborting(FALSE)
{
	ZeroMemory(&m_threads[0], sizeof(m_threads));
	m_pDiffParm = new DiffFuncStruct;
	m_pAbortgate = new DiffThreadAbortable(this);
}

CDiffThread::~CDiffThread()
{
	delete m_pDiffParm;
	delete m_pAbortgate;
}

/**
 * @brief Sets context pointer forwarded to thread
 */
void CDiffThread::SetContext(CDiffContext * pCtx)
{
	m_pDiffContext = pCtx;
}

/**
 * @brief Start directory compare thread
 */
UINT CDiffThread::CompareDirectories(const CString & dir1,
		const CString & dir2, BOOL bRecursive)
{
	ASSERT(m_pDiffParm->nThreadState != THREAD_COMPARING);

	m_pDiffParm->path1 = dir1;
	m_pDiffParm->path2 = dir2;
	m_pDiffParm->bRecursive = bRecursive;
	m_pDiffParm->context = m_pDiffContext;
	m_pDiffParm->msgUIUpdate = m_msgUpdateUI;
	m_pDiffParm->hWindow = m_hWnd;
	m_pDiffParm->m_pAbortgate = m_pAbortgate;
	m_pDiffParm->bOnlyRequested = m_bOnlyRequested;
	m_pDiffParm->pItemList = &m_diffList;
	m_bAborting = FALSE;

	m_pDiffParm->nThreadState = THREAD_COMPARING;

	if (bSinglethreaded)
	{
		DiffThreadCollect(m_pDiffParm);
		DiffThreadCompare(m_pDiffParm);
	}
	else
	{
		m_threads[0] = AfxBeginThread(DiffThreadCollect, m_pDiffParm);
		m_threads[1] = AfxBeginThread(DiffThreadCompare, m_pDiffParm);
	}

	return 1;
}

/**
 * @brief Set window receiving messages thread sends
 */
void CDiffThread::SetHwnd(HWND hWnd)
{
	m_hWnd = hWnd;
}

/**
 * @brief Set message-id and -number for messages thread sends to window
 */
void CDiffThread::SetMessageIDs(UINT updateMsg, UINT statusMsg)
{
	m_msgUpdateUI = updateMsg;
}

/**
 * @brief Selects to compare all or only selected items.
 * @param [in] bSelected If TRUE only selected items are compared.
 */
void CDiffThread::SetCompareSelected(bool bSelected /*=FALSE*/)
{
	m_bOnlyRequested = bSelected;
}

/**
 * @brief Returns thread's current state
 */
UINT CDiffThread::GetThreadState() const
{
	return m_pDiffParm->nThreadState;
}

/**
 * @brief Item collection thread function.
 *
 * This thread is responsible for finding and collecting all items to compare
 * to the item list.
 * @param [in] lParam Pointer to parameter structure.
 * @return Thread's return value.
 */
UINT DiffThreadCollect(LPVOID lpParam)
{
	PathContext paths;
	DiffFuncStruct *myStruct = (DiffFuncStruct *) lpParam;
	HWND hWnd = myStruct->hWindow;
	UINT msgID = myStruct->msgUIUpdate;
	bool bOnlyRequested = myStruct->bOnlyRequested;
	myStruct->context->m_bCollectReady = FALSE;

	// Stash abortable interface into context
	myStruct->context->SetAbortable(myStruct->m_pAbortgate);

	// keep the scripts alive during the Rescan
	// when we exit the thread, we delete this and release the scripts
	CAssureScriptsForThread scriptsForRescan;

	bool casesensitive = false;
	int depth = myStruct->bRecursive ? -1 : 0;

	paths.SetLeft(myStruct->context->GetNormalizedLeft());
	paths.SetRight(myStruct->context->GetNormalizedRight());
	if (bOnlyRequested)
	{
		myStruct->context->m_pCompareStats->SetCompareState(CompareStats::STATE_COMPARE);
		DirScan_CompareItems(myStruct->context);
		myStruct->context->m_pCompareStats->SetCompareState(CompareStats::STATE_IDLE);
	}
	else
	{
		myStruct->context->m_pCompareStats->SetCompareState(CompareStats::STATE_COLLECT);
		CString subdir; // blank to start at roots specified in diff context
#ifdef _DEBUG
		_CrtMemState memStateBefore;
		_CrtMemState memStateAfter;
		_CrtMemState memStateDiff;
		_CrtMemCheckpoint(&memStateBefore);
#endif

		// Build results list (except delaying file comparisons until below)
		DirScan_GetItems(paths, subdir, subdir, myStruct->pItemList, casesensitive, depth,  myStruct->context);

#ifdef _DEBUG
		_CrtMemCheckpoint(&memStateAfter);
		_CrtMemDifference(&memStateDiff, &memStateBefore, &memStateAfter);
		_CrtMemDumpStatistics(&memStateDiff);
#endif
	}

	// Signal that collect phase is ready
	myStruct->context->m_bCollectReady = TRUE;
	return 1;
}


/**
 * @brief Folder compare thread function.
 *
 * Compares items in item list. After compare is ready
 * sends message to UI so UI can update itself.
 * @param [in] lParam Pointer to parameter structure.
 * @return Thread's return value.
 */
UINT DiffThreadCompare(LPVOID lpParam)
{
	DiffFuncStruct *myStruct = (DiffFuncStruct *) lpParam;
	HWND hWnd = myStruct->hWindow;
	UINT msgID = myStruct->msgUIUpdate;
	bool bOnlyRequested = myStruct->bOnlyRequested;

	// Stash abortable interface into context
	myStruct->context->SetAbortable(myStruct->m_pAbortgate);

	// keep the scripts alive during the Rescan
	// when we exit the thread, we delete this and release the scripts
	CAssureScriptsForThread scriptsForRescan;

	if (bOnlyRequested)
	{
		myStruct->context->m_pCompareStats->SetCompareState(CompareStats::STATE_COMPARE);
		DirScan_CompareItems(myStruct->context);
		myStruct->context->m_pCompareStats->SetCompareState(CompareStats::STATE_IDLE);
	}
	else
	{
		myStruct->context->m_pCompareStats->SetCompareState(CompareStats::STATE_COMPARE);

		// Now do all pending file comparisons
		DirScan_CompareItems(myStruct->pItemList, myStruct->context);

		myStruct->context->m_pCompareStats->SetCompareState(CompareStats::STATE_IDLE);
	}

	// Clear the list - its job is now done
	myStruct->pItemList->RemoveAll();

	// Send message to UI to update
	myStruct->nThreadState = THREAD_COMPLETED;
	// msgID=MSG_UI_UPDATE=1025 (2005-11-29, Perry)
	PostMessage(hWnd, msgID, NULL, NULL);
	return 1;
}
