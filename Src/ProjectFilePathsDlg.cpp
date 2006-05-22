/**
 * @file  ProjectFilePathsDlg.cpp
 *
 * @brief Implementation file for ProjectFilePaths dialog
 */
// RCS ID line follows -- this is updated by CVS
// $Id$

#include "stdafx.h"
#include "Merge.h"
#include "MainFrm.h"
#include "paths.h"
#include "OptionsDef.h"
#include "ProjectFile.h"
#include "coretools.h"
#include "ProjectFilePathsDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNCREATE(ProjectFilePathsDlg, CPropertyPage)

/** 
 * @brief Standard constructor.
 */
ProjectFilePathsDlg::ProjectFilePathsDlg() : CPropertyPage(ProjectFilePathsDlg::IDD)
{
	//{{AFX_DATA_INIT(ProjectFilePathsDlg)
	//}}AFX_DATA_INIT
}

void ProjectFilePathsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(ProjectFilePathsDlg)
	DDX_Text(pDX, IDC_PROJ_LFILE_EDIT, m_sLeftFile);
	DDX_Text(pDX, IDC_PROJ_RFILE_EDIT, m_sRightFile);
	DDX_Text(pDX, IDC_PROJ_FILTER_EDIT, m_sFilter);
	DDX_Check(pDX, IDC_PROJ_INC_SUBFOLDERS, m_bIncludeSubfolders);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(ProjectFilePathsDlg, CDialog)
	//{{AFX_MSG_MAP(ProjectFilePathsDlg)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_PROJ_LFILE_BROWSE, OnBnClickedProjLfileBrowse)
	ON_BN_CLICKED(IDC_PROJ_RFILE_BROWSE, OnBnClickedProjRfileBrowse)
	ON_BN_CLICKED(IDC_PROJ_FILTER_SELECT, OnBnClickedProjFilterSelect)
	ON_BN_CLICKED(IDC_PROJ_OPEN, OnBnClickedProjOpen)
	ON_BN_CLICKED(IDC_PROJ_SAVE, OnBnClickedProjSave)
END_MESSAGE_MAP()

/** 
 * @brief Initialize dialog.
 */
BOOL ProjectFilePathsDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	return FALSE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

/** 
 * @brief Called when Browse-button for left path is selected.
 */
void ProjectFilePathsDlg::OnBnClickedProjLfileBrowse()
{
	UpdateData(TRUE);
	if (::SelectFile(m_sLeftFile, m_sLeftFile, NULL, NULL, TRUE))
		UpdateData(FALSE);
}

/** 
 * @brief Called when Browse-button for right path is selected.
 */
void ProjectFilePathsDlg::OnBnClickedProjRfileBrowse()
{
	UpdateData(TRUE);
	if (::SelectFile(m_sRightFile, m_sRightFile, NULL, NULL, TRUE))
		UpdateData(FALSE);
}

/** 
 * @brief Called when Selec-button for filter is selected.
 */
void ProjectFilePathsDlg::OnBnClickedProjFilterSelect()
{
	CString filterPrefix;
	CString curFilter;
	VERIFY(filterPrefix.LoadString(IDS_FILTER_PREFIX));

	const BOOL bUseMask = theApp.m_globalFileFilter.IsUsingMask();
	GetDlgItemText(IDC_PROJ_FILTER_EDIT, curFilter);
	curFilter.TrimLeft();
	curFilter.TrimRight();

	GetMainFrame()->SelectFilter();
	
	CString filterNameOrMask = theApp.m_globalFileFilter.GetFilterNameOrMask();
	if (theApp.m_globalFileFilter.IsUsingMask())
	{
		// If we had filter chosen and now has mask we can overwrite filter
		if (!bUseMask || curFilter[0] != '*')
		{
			SetDlgItemText(IDC_PROJ_FILTER_EDIT, filterNameOrMask);
		}
	}
	else
	{
		filterNameOrMask.Insert(0, filterPrefix);
		SetDlgItemText(IDC_PROJ_FILTER_EDIT, filterNameOrMask);
	}
}

/** 
 * @brief Callled when Open-button for project file is selected.
 */
void ProjectFilePathsDlg::OnBnClickedProjOpen()
{
	CString fileName = AskProjectFileName(TRUE);
	if (fileName.IsEmpty())
		return;

	ProjectFile project;

	CString sErr;
	if (!project.Read(fileName, &sErr))
	{
		if (sErr.IsEmpty())
			sErr = LoadResString(IDS_UNK_ERROR_SAVING_PROJECT);
		CString msg;
		AfxFormatString2(msg, IDS_ERROR_FILEOPEN, fileName, sErr);
		AfxMessageBox(msg, MB_ICONSTOP);
	}
	else
	{
		m_sLeftFile = project.GetLeft();
		m_sRightFile = project.GetRight();
		m_sFilter = project.GetFilter();
		m_bIncludeSubfolders = project.GetSubfolders();
	}
	UpdateData(FALSE);
}

/** 
 * @brief Called when Save-button for project file is selected.
 */
void ProjectFilePathsDlg::OnBnClickedProjSave()
{
	UpdateData(TRUE);

	CString fileName = AskProjectFileName(FALSE);
	if (fileName.IsEmpty())
		return;

	ProjectFile project;

	if (!m_sLeftFile.IsEmpty())
		project.SetLeft(m_sLeftFile);
	if (!m_sRightFile.IsEmpty())
		project.SetRight(m_sRightFile);
	if (!m_sFilter.IsEmpty())
		project.SetFilter(m_sFilter);
    project.SetSubfolders(m_bIncludeSubfolders);

	CString sErr;
	if (!project.Save(fileName, &sErr))
	{
		if (sErr.IsEmpty())
			sErr = LoadResString(IDS_UNK_ERROR_SAVING_PROJECT);
		CString msg;
		AfxFormatString2(msg, IDS_ERROR_FILEOPEN, fileName, sErr);
		AfxMessageBox(msg, MB_ICONSTOP);
	}
}

/** 
 * @brief Set paths for dialog.
 * This function is used to set paths from outside the dialog.
 * @param [in] left Left path.
 * @param [in] right Right path.
 */
void ProjectFilePathsDlg::SetPaths(LPCTSTR left, LPCTSTR right)
{
	if (left != NULL)
		m_sLeftFile = left;
	if (right != NULL)
		m_sRightFile = right;
}

/** 
 * @brief Allow user to select a file to open/save.
 */
CString ProjectFilePathsDlg::AskProjectFileName(BOOL bOpen)
{
	// get the default projects path
	CString strProjectFileName;
	CString strProjectPath = GetOptionsMgr()->GetString(OPT_PROJECTS_PATH);

	if (!::SelectFile(strProjectFileName, strProjectPath, NULL, IDS_PROJECTFILES, bOpen))
		return _T("");

	if (strProjectFileName.IsEmpty())
		return _T("");

	// Add projectfile extension if it is missing
	// So we allow 'filename.otherext' but add extension for 'filename'
	CString filename;
	CString extension;
	SplitFilename(strProjectFileName, NULL, &filename, &extension);
	if (extension.IsEmpty())
	{
		CString projectFileExt;
		projectFileExt.LoadString(IDS_PROJECTFILES_EXT);
		strProjectFileName += _T(".");
		strProjectFileName += projectFileExt;
	}

	// get the path part from the filename
	strProjectPath = paths_GetParentPath(strProjectFileName);
	// store this as the new project path
	GetOptionsMgr()->SaveOption(OPT_PROJECTS_PATH, strProjectPath);
	return strProjectFileName;
}
