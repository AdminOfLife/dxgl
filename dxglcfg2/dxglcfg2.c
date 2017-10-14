// DXGL
// Copyright (C) 2011-2017 William Feely

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#define _WIN32_WINNT 0x0600
#define _WIN32_IE 0x0300
#define _CRT_SECURE_NO_WARNINGS
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <windows.h>
#include <HtmlHelp.h>
#include <CommCtrl.h>
#include <string.h>
#include <tchar.h>
#include <stdio.h>
#include <float.h>
#include <math.h>
#include <io.h>
#include <Uxtheme.h>
#include <Vsstyle.h>
#include "resource.h"
#include "../cfgmgr/LibSha256.h"
#include "../cfgmgr/cfgmgr.h"
#include <gl/GL.h>

#ifndef SHGFI_ADDOVERLAYS
#define SHGFI_ADDOVERLAYS 0x000000020
#endif //SHGFI_ADDOVERLAYS

#ifndef BCM_SETSHIELD
#define BCM_SETSHIELD 0x160C
#endif

#define GL_TEXTURE_MAX_ANISOTROPY_EXT          0x84FE
#define GL_MAX_SAMPLES_EXT                     0x8D57
#define GL_MAX_MULTISAMPLE_COVERAGE_MODES_NV        0x8E11
#define GL_MULTISAMPLE_COVERAGE_MODES_NV            0x8E12
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT      0x84FF

DXGLCFG *cfg;
DXGLCFG *cfgmask;
BOOL *dirty;
HINSTANCE hinstance;
BOOL msaa = FALSE;
const char *extensions_string = NULL;
OSVERSIONINFO osver;
TCHAR hlppath[MAX_PATH+16];
HMODULE uxtheme = NULL;
HTHEME hThemeDisplay = NULL;
HTHEME(WINAPI *_OpenThemeData)(HWND hwnd, LPCWSTR pszClassList) = NULL;
HRESULT(WINAPI *_CloseThemeData)(HTHEME hTheme) = NULL;
HRESULT(WINAPI *_DrawThemeBackground)(HTHEME hTheme, HDC hdc, int iPartID,
	int iStateID, const RECT *pRect, const RECT *pClipRect) = NULL;
HRESULT(WINAPI *_EnableThemeDialogTexture)(HWND hwnd, DWORD dwFlags) = NULL;
static BOOL ExtraModes_Dropdown = FALSE;
static BOOL ColorDepth_Dropdown = FALSE;
static HWND hDialog = NULL;


typedef struct
{
	LPTSTR regkey;
	LPTSTR name;
	HICON icon;
	BOOL icon_shared;
	BOOL dirty;
	DXGLCFG cfg;
	DXGLCFG mask;
	TCHAR path[MAX_PATH];
	BOOL builtin;
} app_setting;

TCHAR exe_filter[] = _T("Program Files\0*.exe\0All Files\0*.*\0\0");

app_setting *apps;
int appcount;
int maxapps;
DWORD current_app;
BOOL tristate;
TCHAR strdefault[] = _T("(global default)");
HWND hTab;
HWND hTabs[6];
int tabopen;

static const TCHAR *colormodes[32] = {
	_T("None"),
	_T("8-bit"),
	_T("15-bit"),
	_T("8/15-bit"),
	_T("16-bit"),
	_T("8/16-bit"),
	_T("8/15-bit"),
	_T("8/15/16-bit"),
	_T("24-bit"),
	_T("8/24-bit"),
	_T("15/24-bit"),
	_T("8/15/24-bit"),
	_T("16/24-bit"),
	_T("8/16/24-bit"),
	_T("8/15/24-bit"),
	_T("8/15/16/24-bit"),
	_T("32-bit"),
	_T("8/32-bit"),
	_T("15/32-bit"),
	_T("8/15/32-bit"),
	_T("16/32-bit"),
	_T("8/16/32-bit"),
	_T("8/15/32-bit"),
	_T("8/15/16/32-bit"),
	_T("24/32-bit"),
	_T("8/24/32-bit"),
	_T("15/24/32-bit"),
	_T("8/15/24/32-bit"),
	_T("16/24/32-bit"),
	_T("8/16/24/32-bit"),
	_T("8/15/24/32-bit"),
	_T("8/15/16/24/32-bit")
};

static const TCHAR *colormodedropdown[5] = {
	_T("8-bit"),
	_T("15-bit"),
	_T("16-bit"),
	_T("24-bit"),
	_T("32-bit")
};

static const TCHAR *extramodes[7] = {
	_T("Common low resolutions"),
	_T("Uncommon low resolutions"),
	_T("Uncommon SD resolutions"),
	_T("High Definition resolutions"),
	_T("Ultra-HD resolutions"),
	_T("Ultra-HD above 4k"),
	_T("Very uncommon resolutions")
};

DWORD AddApp(LPCTSTR path, BOOL copyfile, BOOL admin, BOOL force, HWND hwnd)
{
	BOOL installed = FALSE;
	BOOL dxgl_installdir = FALSE;
	BOOL old_dxgl = TRUE;
	BOOL backupped = FALSE;
	TCHAR command[MAX_PATH + 37];
	SHELLEXECUTEINFO shex;
	DWORD exitcode;
	app_ini_options inioptions;
	HMODULE hmod;
	if (copyfile)
	{
		DWORD sizeout = (MAX_PATH + 1) * sizeof(TCHAR);
		TCHAR installpath[MAX_PATH + 1];
		TCHAR srcpath[MAX_PATH + 1];
		TCHAR inipath[MAX_PATH + 1];
		TCHAR backuppath[MAX_PATH + 1];
		TCHAR destpath[MAX_PATH + 1];
		HKEY hKeyInstall;
		LONG error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\DXGL"), 0, KEY_READ, &hKeyInstall);
		if (error == ERROR_SUCCESS)
		{
			dxgl_installdir = TRUE;
			error = RegQueryValueEx(hKeyInstall, _T("InstallDir"), NULL, NULL, (LPBYTE)installpath, &sizeout);
			if (error == ERROR_SUCCESS) installed = TRUE;
		}
		if (hKeyInstall) RegCloseKey(hKeyInstall);
		if (!installed)
		{
			GetModuleFileName(NULL, installpath, MAX_PATH + 1);
		}
		if (dxgl_installdir) _tcscat(installpath, _T("\\"));
		else (_tcsrchr(installpath, _T('\\')))[1] = 0;
		_tcsncpy(srcpath, installpath, MAX_PATH + 1);
		_tcscat(srcpath, _T("ddraw.dll"));
		_tcsncpy(destpath, path, MAX_PATH + 1);
		(_tcsrchr(destpath, _T('\\')))[1] = 0;
		_tcscat(destpath, _T("ddraw.dll"));
		_tcsncpy(backuppath, path, MAX_PATH + 1);
		(_tcsrchr(backuppath, _T('\\')))[1] = 0;
		_tcscat(backuppath, _T("ddraw.dll.dxgl-backup"));
		_tcsncpy(inipath, path, MAX_PATH + 1);
		(_tcsrchr(inipath, _T('\\')))[1] = 0;
		// Check for DXGL ini file and existing ddraw.dll
		ReadAppINIOptions(inipath, &inioptions);
		error = CopyFile(srcpath, destpath, TRUE);
	error_loop:
		if (!error)
		{
			error = GetLastError();
			if (error == ERROR_FILE_EXISTS)
			{
				if (inioptions.NoOverwrite)
				{
					MessageBox(hwnd, _T("Cannot install DXGL.  An INI file has \
been placed in your game folder prohibiting overwriting the existing DirectDraw \
library.\r\n\r\nIf you want to install DXGL, edit the dxgl.ini file in your game \
folder and set the NoOverwite value to false.\r\n\r\n\
A profile will still be created for your game but may not be compatible with the \
DirectDraw library in your game folder."), _T("Error"), MB_OK | MB_ICONERROR);
					return 0; // Continue to install registry key anyway
				}
				if ((inioptions.sha256[0] != 0) && !memcmp(inioptions.sha256, inioptions.sha256comp, 64))
					// Detected original ddraw matches INI hash
				{
					error = CopyFile(destpath, backuppath, FALSE);
					if (!error)
					{
						error = GetLastError();
						if ((error == ERROR_ACCESS_DENIED) && !admin)
						{
							_tcscpy(command, _T(" install "));
							_tcscat(command, path);
							ZeroMemory(&shex, sizeof(SHELLEXECUTEINFO));
							shex.cbSize = sizeof(SHELLEXECUTEINFO);
							shex.lpVerb = _T("runas");
							shex.fMask = SEE_MASK_NOCLOSEPROCESS;
							_tcscat(installpath, _T("\\dxglcfg.exe"));
							shex.lpFile = installpath;
							shex.lpParameters = command;
							ShellExecuteEx(&shex);
							WaitForSingleObject(shex.hProcess, INFINITE);
							GetExitCodeProcess(shex.hProcess, &exitcode);
							return exitcode;
						}
					}
					else backupped = TRUE;
				}
				error = SetErrorMode(SEM_FAILCRITICALERRORS);
				SetErrorMode(error | SEM_FAILCRITICALERRORS);
				hmod = LoadLibrary(destpath);
				SetErrorMode(error);
				if(hmod)
				{
					if(GetProcAddress(hmod,"IsDXGLDDraw") || force) old_dxgl = TRUE;
					else old_dxgl = FALSE;
					FreeLibrary(hmod);
				}
				else
				{
					if (force) old_dxgl = TRUE;
					else old_dxgl = FALSE;
				}
				if(old_dxgl)
				{
					error = CopyFile(srcpath,destpath,FALSE);
					goto error_loop;
				}
				else
				{
					// Prompt to overwrite
					if (MessageBox(hwnd, _T("A custom DirectDraw library has been detected in \
your game folder.  Would you like to replace it with DXGL?\r\n\r\n\
Warning:  Installing DXGL will remove any customizations that the existing custom DirectDraw \
library may have."), _T("DXGL Config"), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
					{
						error = CopyFile(srcpath, destpath, FALSE);
						goto error_loop;
					}
					else
					{
						if (backupped) DeleteFile(backuppath);
					}
				}
			}
			if((error == ERROR_ACCESS_DENIED) && !admin)
			{
				if(old_dxgl) _tcscpy(command,_T(" install "));
				else _tcscpy(command, _T(" forceinstall "));
				_tcscat(command,path);
				ZeroMemory(&shex,sizeof(SHELLEXECUTEINFO));
				shex.cbSize = sizeof(SHELLEXECUTEINFO);
				shex.lpVerb = _T("runas");
				shex.fMask = SEE_MASK_NOCLOSEPROCESS;
				_tcscat(installpath,_T("\\dxglcfg.exe"));
				shex.lpFile = installpath;
				shex.lpParameters = command;
				ShellExecuteEx(&shex);
				WaitForSingleObject(shex.hProcess,INFINITE);
				GetExitCodeProcess(shex.hProcess,&exitcode);
				return exitcode;
			}
			return error;
		}
	}
	return 0;
}

DWORD DelApp(LPCTSTR path, BOOL admin, HWND hwnd)
{
	BOOL installed = FALSE;
	TCHAR command[MAX_PATH + 32];
	BOOL old_dxgl = TRUE;
	DWORD sizeout = (MAX_PATH+1)*sizeof(TCHAR);
	TCHAR installpath[MAX_PATH+1];
	TCHAR inipath[MAX_PATH + 1];
	TCHAR backuppath[MAX_PATH + 1];
	HKEY hKeyInstall;
	HMODULE hmod;
	SHELLEXECUTEINFO shex;
	DWORD exitcode;
	HANDLE exists;
	app_ini_options inioptions;
	LONG error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\DXGL"), 0, KEY_READ, &hKeyInstall);
	if(error == ERROR_SUCCESS)
	{
		error = RegQueryValueEx(hKeyInstall,_T("InstallDir"),NULL,NULL,(LPBYTE)installpath,&sizeout);
		if(error == ERROR_SUCCESS) installed = TRUE;
	}
	if(hKeyInstall) RegCloseKey(hKeyInstall);
	if(!installed)
	{
		GetModuleFileName(NULL,installpath,MAX_PATH+1);
	}
	_tcsncpy(inipath, path, MAX_PATH + 1);
	(_tcsrchr(inipath, _T('\\')))[1] = 0;
	_tcsncpy(backuppath, path, MAX_PATH + 1);
	(_tcsrchr(backuppath, _T('\\')))[1] = 0;
	_tcscat(backuppath, _T("ddraw.dll.dxgl-backup"));
	// Check for DXGL ini file and existing ddraw.dll
	ReadAppINIOptions(inipath, &inioptions);
	if (inioptions.NoOverwrite || inioptions.NoUninstall)
	{
		MessageBox(hwnd,_T("DXGL has not been removed from your game folder.  \
An INI file has been found in your game folder prohibiting the DirectDraw \
library in your game folder from being deleted.\r\n\r\n\
If this is in error, you will have to manually delete ddraw.dll from your \
game folder.  If your game was distributed by Steam or a similar service \
please verify your game files after removing the file, in case the game \
shipped with a custom DirectDraw library."), _T("Warning"), MB_OK | MB_ICONWARNING);
		return 0;  // Continue to delete registry profile.
	}
	error = SetErrorMode(SEM_FAILCRITICALERRORS);
	SetErrorMode(error | SEM_FAILCRITICALERRORS);
	hmod = LoadLibrary(path);
	SetErrorMode(error);
	if(hmod)
	{
		if(!GetProcAddress(hmod,"IsDXGLDDraw")) old_dxgl = FALSE;
		FreeLibrary(hmod);
	}
	else old_dxgl = FALSE;
	if(!old_dxgl) return 0;
	if(!DeleteFile(path))
	{
		error = GetLastError();
		if((error == ERROR_ACCESS_DENIED) && !admin)
		{
			_tcscpy(command,_T(" remove "));
			_tcscat(command,path);
			ZeroMemory(&shex,sizeof(SHELLEXECUTEINFO));
			shex.cbSize = sizeof(SHELLEXECUTEINFO);
			shex.lpVerb = _T("runas");
			shex.fMask = SEE_MASK_NOCLOSEPROCESS;
			_tcscat(installpath,_T("\\dxglcfg.exe"));
			shex.lpFile = installpath;
			shex.lpParameters = command;
			ShellExecuteEx(&shex);
			WaitForSingleObject(shex.hProcess,INFINITE);
			GetExitCodeProcess(shex.hProcess,&exitcode);
			return exitcode;
		}
		else if (error != ERROR_FILE_NOT_FOUND) return error;
	}
	exists = CreateFile(backuppath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (exists == INVALID_HANDLE_VALUE) return 0;
	else
	{
		CloseHandle(exists);
		error = MoveFile(backuppath, path);
		if (!error)
		{
			error = GetLastError();
			if ((error == ERROR_ACCESS_DENIED) && !admin)
			{
				_tcscpy(command, _T(" remove "));
				_tcscat(command, path);
				ZeroMemory(&shex, sizeof(SHELLEXECUTEINFO));
				shex.cbSize = sizeof(SHELLEXECUTEINFO);
				shex.lpVerb = _T("runas");
				shex.fMask = SEE_MASK_NOCLOSEPROCESS;
				_tcscat(installpath, _T("\\dxglcfg.exe"));
				shex.lpFile = installpath;
				shex.lpParameters = command;
				ShellExecuteEx(&shex);
				WaitForSingleObject(shex.hProcess, INFINITE);
				GetExitCodeProcess(shex.hProcess, &exitcode);
				return exitcode;
			}
			else return error;
		}
	}
	return 0;
}

void SaveChanges(HWND hWnd)
{
	int i;
	if(apps[0].dirty) SetGlobalConfig(&apps[0].cfg);
	for(i = 1; i < appcount; i++)
	{
		if(apps[i].dirty) SetConfig(&apps[i].cfg,&apps[i].mask,apps[i].regkey);
	}
	EnableWindow(GetDlgItem(hWnd,IDC_APPLY),FALSE);
}

void FloatToAspect(float f, LPTSTR aspect)
{
	double integer;
	double dummy;
	float fract;
	TCHAR denominator[5];
	int i;
	if (_isnan(f)) f = 0.0f; //Handle NAN condition
	if (f >= 1000.0f)  // Clamp ridiculously wide aspects
	{
		_tcscpy(aspect, _T("1000:1"));
		return;
	}
	if (f < 0.001f)   // Exclude ridiculously tall aspects, zero, and negative
	{
		_tcscpy(aspect, _T("Default"));
		return;
	}
	// Handle common aspects
	if (fabsf(f - 1.25f) < 0.0001f)
	{
		_tcscpy(aspect, _T("5:4"));
		return;
	}
	if (fabsf(f - 1.3333333f) < 0.0001f)
	{
		_tcscpy(aspect, _T("4:3"));
		return;
	}
	if (fabsf(f - 1.6f) < 0.0001f)
	{
		_tcscpy(aspect, _T("16:10"));
		return;
	}
	if (fabsf(f - 1.7777777) < 0.0001f)
	{
		_tcscpy(aspect, _T("16:9"));
		return;
	}
	if (fabsf(f - 1.9333333) < 0.0001f)
	{
		_tcscpy(aspect, _T("256:135"));
		return;
	}
	fract = modff(f, &integer);
	if (fract < 0.0001f)  //Handle integer aspects
	{
		_itot((int)integer, aspect, 10);
		_tcscat(aspect, _T(":1"));
		return;
	}
	// Finally try from 2 to 1000
	for (i = 2; i < 1000; i++)
	{
		if (fabsf(modff(fract*i, &dummy)) < 0.0001f)
		{
			_itot((f*i) + .5f, aspect, 10);
			_itot(i, denominator, 10);
			_tcscat(aspect, _T(":"));
			_tcscat(aspect, denominator);
			return;
		}
	}
	// Cannot find a reasonable fractional aspect, so display as decimal.
#ifdef _UNICODE
	swprintf(aspect, 31, L"%.6g", f);
#else
	sprintf(aspect,"%.6g", f);
#endif
}

void FloatToScale(float x, float y, LPTSTR scale)
{
	TCHAR numberx[8];
	TCHAR numbery[8];
	if (_isnan(x)) x = 0.0f; //Handle NAN condition
	if (_isnan(y)) y = 0.0f;
	// Too low number, round to "Auto"
	if (x < 0.25f) x = 0.0f;
	if (y < 0.25f) y = 0.0f;
	// Too high number, round to 16
	if (x > 16.0f) x = 16.0f;
	if (y > 16.0f) y = 16.0f;
	// Test if either scale is zero
	if ((x == 0) || (y == 0))
	{
		_tcscpy(scale, _T("Auto"));
		return;
	}
	// Write numbers
#ifdef _UNICODE
	swprintf(numberx, 7, L"%.4g", x);
	swprintf(numbery, 7, L"%.4g", y);
#else
	sprintf(numberx, ".4g", x);
	sprintf(numbery, ".4g", y);
#endif
	// Fill out string
	_tcscpy(scale, numberx);
	_tcscat(scale, _T("x"));
	if (x != y) _tcscat(scale, numbery);
}

void SetCheck(HWND hWnd, int DlgItem, BOOL value, BOOL mask, BOOL tristate)
{
	if(tristate && !mask)
		SendDlgItemMessage(hWnd,DlgItem,BM_SETCHECK,BST_INDETERMINATE,0);
	else
	{
		if(value) SendDlgItemMessage(hWnd,DlgItem,BM_SETCHECK,BST_CHECKED,0);
		else SendDlgItemMessage(hWnd,DlgItem,BM_SETCHECK,BST_UNCHECKED,0);
	}
}

void SetCombo(HWND hWnd, int DlgItem, DWORD value, DWORD mask, BOOL tristate)
{
	if(tristate && !mask)
		SendDlgItemMessage(hWnd,DlgItem,CB_SETCURSEL,
		SendDlgItemMessage(hWnd,DlgItem,CB_FINDSTRING,-1,(LPARAM)strdefault),0);
	else
		SendDlgItemMessage(hWnd,DlgItem,CB_SETCURSEL,value,0);
}

__inline DWORD EncodePrimaryScale(DWORD scale)
{
	switch (scale)
	{
	case 0:
		return 2;
	case 1:
		return 0;
	case 2:
		return 1;
	default:
		return scale;
	}
}

void SetPrimaryScaleCombo(HWND hWnd, int DlgItem, DWORD value, DWORD mask, BOOL tristate)
{
	if (tristate && !mask)
		SendDlgItemMessage(hWnd, DlgItem, CB_SETCURSEL,
		SendDlgItemMessage(hWnd, DlgItem, CB_FINDSTRING, -1, (LPARAM)strdefault), 0);
	else
		SendDlgItemMessage(hWnd, DlgItem, CB_SETCURSEL, EncodePrimaryScale(value), 0);
}

void SetAspectCombo(HWND hWnd, int DlgItem, float value, DWORD mask, BOOL tristate)
{
	TCHAR buffer[32];
	if (tristate && !mask)
		SendDlgItemMessage(hWnd, DlgItem, CB_SETCURSEL,
			SendDlgItemMessage(hWnd, DlgItem, CB_FINDSTRING, -1, (LPARAM)strdefault), 0);
	else
	{
		FloatToAspect(value, buffer);
		SendDlgItemMessage(hWnd,DlgItem,CB_SETCURSEL,
			SendDlgItemMessage(hWnd, DlgItem, CB_FINDSTRING, -1, (LPARAM)buffer), 0);
		SetDlgItemText(hWnd, DlgItem, buffer);
	}
}

void SetPostScaleCombo(HWND hWnd, int DlgItem, float x, float y, DWORD maskx, DWORD masky, BOOL tristate)
{
	TCHAR buffer[32];
	if (tristate && !maskx && !masky)
		SendDlgItemMessage(hWnd, DlgItem, CB_SETCURSEL,
			SendDlgItemMessage(hWnd, DlgItem, CB_FINDSTRING, -1, (LPARAM)strdefault), 0);
	else
	{
		FloatToScale(x, y, buffer);
		SendDlgItemMessage(hWnd, DlgItem, CB_SETCURSEL,
			SendDlgItemMessage(hWnd, DlgItem, CB_FINDSTRING, -1, (LPARAM)buffer), 0);
		SetDlgItemText(hWnd, DlgItem, buffer);
	}
}

void SetText(HWND hWnd, int DlgItem, TCHAR *value, TCHAR *mask, BOOL tristate)
{
	if(tristate && (mask[0] == 0))
		SetWindowText(GetDlgItem(hWnd,DlgItem),_T(""));
	else SetWindowText(GetDlgItem(hWnd,DlgItem),value);
}

BOOL GetCheck(HWND hWnd, int DlgItem, BOOL *mask)
{
	int check = SendDlgItemMessage(hWnd,DlgItem,BM_GETCHECK,0,0);
	switch(check)
	{
	case BST_CHECKED:
		*mask = TRUE;
		return TRUE;
	case BST_UNCHECKED:
		*mask = TRUE;
		return FALSE;
	case BST_INDETERMINATE:
	default:
		*mask = FALSE;
		return FALSE;
	}
}

DWORD GetCombo(HWND hWnd, int DlgItem, DWORD *mask)
{
	int value = SendDlgItemMessage(hWnd,DlgItem,CB_GETCURSEL,0,0);
	if(value == SendDlgItemMessage(hWnd,DlgItem,CB_FINDSTRING,-1,(LPARAM)strdefault))
	{
		*mask = 0;
		return 0;
	}
	else
	{
		*mask = 1;
		return value;
	}
}

void GetPostScaleCombo(HWND hWnd, int DlgItem, float *x, float *y, float *maskx, float *masky)
{
	TCHAR buffer[32];
	TCHAR *ptr;
	GetDlgItemText(hWnd, DlgItem, buffer, 31);
	buffer[31] = 0;
	if (!_tcscmp(buffer, strdefault))
	{
		*maskx = 0.0f;
		*masky = 0.0f;
		*x = 0.0f;
		*y = 0.0f;
		return;
	}
	else
	{
		*maskx = 1.0f;
		*masky = 1.0f;
		// Check for Auto
		if (!_tcsicmp(buffer, _T("Auto)")))
		{
			*x = 0.0f;
			*y = 0.0f;
			return;
		}
		else
		{
			// Check for certain characters
			ptr = _tcsstr(buffer, _T("x"));
			if (!ptr) ptr = _tcsstr(buffer, _T("X"));
			if (!ptr) ptr = _tcsstr(buffer, _T(","));
			if (!ptr) ptr = _tcsstr(buffer, _T("-"));
			if (!ptr) ptr = _tcsstr(buffer, _T(":"));
			if (ptr)
			{
				*ptr = 0;
				*x = _ttof(buffer);
				*y = _ttof(ptr + 1);
				if ((*x >= 0.25f) && (*y < 0.25f)) *y = *x;
				return;
			}
			else
			{
				*x = _ttof(buffer);
				*y = _ttof(buffer);
				return;
			}
		}
	}
}

float GetAspectCombo(HWND hWnd, int DlgItem, float *mask)
{
	TCHAR buffer[32];
	TCHAR *ptr;
	float numerator, denominator;
	GetDlgItemText(hWnd, DlgItem, buffer, 31);
	buffer[31] = 0;
	if (!_tcscmp(buffer, strdefault))
	{
		*mask = 0.0f;
		return 0.0f;
	}
	else
	{
		*mask = 1.0f;
		if (!_tcsicmp(buffer, _T("Default"))) return 0.0f;
		else
		{
			// Check for colon
			ptr = _tcsstr(buffer, _T(":"));
			if (ptr)
			{
				*ptr = 0;
				numerator = (float)_ttof(buffer);
				denominator = (float)_ttof(ptr + 1);
				return numerator / denominator;
			}
			else return (float)_ttof(buffer);
		}
	}
}

void GetText(HWND hWnd, int DlgItem, TCHAR *str, TCHAR *mask)
{
	GetDlgItemText(hWnd,DlgItem,str,MAX_PATH+1);
	if(str[0] == 0) mask[0] = 0;
	else mask[0] = 0xff;
}
LRESULT CALLBACK DisplayTabCallback(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	DRAWITEMSTRUCT* drawitem;
	COLORREF OldTextColor, OldBackColor;
	RECT r;
	TCHAR combotext[64];
	DWORD cursel;
	int i;
	switch (Msg)
	{
	case WM_INITDIALOG:
		if (uxtheme) hThemeDisplay = _OpenThemeData(hWnd, L"Button");
		else hThemeDisplay = NULL;
		if (_EnableThemeDialogTexture) _EnableThemeDialogTexture(hWnd, ETDT_ENABLETAB);
		return TRUE;
	case WM_MEASUREITEM:
		switch (wParam)
		{
		case IDC_COLORDEPTH:
		case IDC_EXTRAMODES:
			((LPMEASUREITEMSTRUCT)lParam)->itemHeight = GetSystemMetrics(SM_CYMENUCHECK);
			((LPMEASUREITEMSTRUCT)lParam)->itemWidth = GetSystemMetrics(SM_CXMENUCHECK);
			break;
		default:
			break;
		}
	case WM_DRAWITEM:
		drawitem = (DRAWITEMSTRUCT*)lParam;
		switch (wParam)
		{
		case IDC_COLORDEPTH:
			OldTextColor = GetTextColor(drawitem->hDC);
			OldBackColor = GetBkColor(drawitem->hDC);
			if ((drawitem->itemState & ODS_SELECTED) && !(drawitem->itemState & ODS_COMBOBOXEDIT))
			{
				SetTextColor(drawitem->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
				SetBkColor(drawitem->hDC, GetSysColor(COLOR_HIGHLIGHT));
				FillRect(drawitem->hDC, &drawitem->rcItem, (HBRUSH)(COLOR_HIGHLIGHT + 1));
			}
			else ExtTextOut(drawitem->hDC, 0, 0, ETO_OPAQUE, &drawitem->rcItem, NULL, 0, NULL);
			memcpy(&r, &drawitem->rcItem, sizeof(RECT));
			if (drawitem->itemID != -1 && !(drawitem->itemState & ODS_COMBOBOXEDIT))
			{
				r.left = r.left + 2;
				r.right = r.left + GetSystemMetrics(SM_CXMENUCHECK);
				if ((cfg->AddColorDepths >> drawitem->itemID) & 1)
				{
					if (hThemeDisplay)
					{
						if (drawitem->itemState & ODS_SELECTED)
							_DrawThemeBackground(hThemeDisplay, drawitem->hDC, BS_AUTOCHECKBOX, CBS_CHECKEDHOT, &r, NULL);
						else _DrawThemeBackground(hThemeDisplay, drawitem->hDC, BS_AUTOCHECKBOX, CBS_CHECKEDNORMAL, &r, NULL);
					}
					else
					{
						if (drawitem->itemState & ODS_SELECTED)
							DrawFrameControl(drawitem->hDC, &r, DFC_BUTTON, DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_HOT);
						else DrawFrameControl(drawitem->hDC, &r, DFC_BUTTON, DFCS_BUTTONCHECK | DFCS_CHECKED);
					}
				}
				else
				{
					if (hThemeDisplay)
					{
						if (drawitem->itemState & ODS_SELECTED)
							_DrawThemeBackground(hThemeDisplay, drawitem->hDC, BS_AUTOCHECKBOX, CBS_UNCHECKEDHOT, &r, NULL);
						else _DrawThemeBackground(hThemeDisplay, drawitem->hDC, BS_AUTOCHECKBOX, CBS_UNCHECKEDNORMAL, &r, NULL);
					}
					else
					{
						if (drawitem->itemState & ODS_SELECTED)
							DrawFrameControl(drawitem->hDC, &r, DFC_BUTTON, DFCS_BUTTONCHECK | DFCS_HOT);
						else DrawFrameControl(drawitem->hDC, &r, DFC_BUTTON, DFCS_BUTTONCHECK);
					}
				}
				drawitem->rcItem.left += GetSystemMetrics(SM_CXMENUCHECK) + 5;
			}
			combotext[0] = 0;
			if (drawitem->itemID != -1 && !(drawitem->itemState & ODS_COMBOBOXEDIT))
				SendDlgItemMessage(hWnd, IDC_COLORDEPTH, CB_GETLBTEXT, drawitem->itemID, combotext);
			else _tcscpy(combotext, colormodes[cfg->AddColorDepths & 31]);
			DrawText(drawitem->hDC, combotext, _tcslen(combotext), &drawitem->rcItem,
				DT_LEFT | DT_SINGLELINE | DT_VCENTER);
			SetTextColor(drawitem->hDC, OldTextColor);
			SetBkColor(drawitem->hDC, OldBackColor);
			if (drawitem->itemID != -1 && !(drawitem->itemState & ODS_COMBOBOXEDIT))
				drawitem->rcItem.left -= GetSystemMetrics(SM_CXMENUCHECK) + 5;
			if (drawitem->itemState & ODS_FOCUS) DrawFocusRect(drawitem->hDC, &drawitem->rcItem);
			DefWindowProc(hWnd, Msg, wParam, lParam);
			break;
		case IDC_EXTRAMODES:
			OldTextColor = GetTextColor(drawitem->hDC);
			OldBackColor = GetBkColor(drawitem->hDC);
			if ((drawitem->itemState & ODS_SELECTED) && !(drawitem->itemState & ODS_COMBOBOXEDIT))
			{
				SetTextColor(drawitem->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
				SetBkColor(drawitem->hDC, GetSysColor(COLOR_HIGHLIGHT));
				FillRect(drawitem->hDC, &drawitem->rcItem, (HBRUSH)(COLOR_HIGHLIGHT + 1));
			}
			else ExtTextOut(drawitem->hDC, 0, 0, ETO_OPAQUE, &drawitem->rcItem, NULL, 0, NULL);
			memcpy(&r, &drawitem->rcItem, sizeof(RECT));
			if (drawitem->itemID != -1 && !(drawitem->itemState & ODS_COMBOBOXEDIT))
			{
				r.left = r.left + 2;
				r.right = r.left + GetSystemMetrics(SM_CXMENUCHECK);
				if ((cfg->AddModes >> drawitem->itemID) & 1)
				{
					if (hThemeDisplay)
					{
						if (drawitem->itemState & ODS_SELECTED)
							_DrawThemeBackground(hThemeDisplay, drawitem->hDC, BS_AUTOCHECKBOX,	CBS_CHECKEDHOT, &r, NULL);
						else _DrawThemeBackground(hThemeDisplay, drawitem->hDC, BS_AUTOCHECKBOX, CBS_CHECKEDNORMAL, &r, NULL);
					}
					else
					{
						if (drawitem->itemState & ODS_SELECTED)
							DrawFrameControl(drawitem->hDC, &r, DFC_BUTTON, DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_HOT);
						else DrawFrameControl(drawitem->hDC, &r, DFC_BUTTON, DFCS_BUTTONCHECK | DFCS_CHECKED);
					}
				}
				else
				{
					if (hThemeDisplay)
					{
						if (drawitem->itemState & ODS_SELECTED)
							_DrawThemeBackground(hThemeDisplay, drawitem->hDC, BS_AUTOCHECKBOX, CBS_UNCHECKEDHOT, &r, NULL);
						else _DrawThemeBackground(hThemeDisplay, drawitem->hDC, BS_AUTOCHECKBOX, CBS_UNCHECKEDNORMAL, &r, NULL);
					}
					else
					{
						if (drawitem->itemState & ODS_SELECTED)
							DrawFrameControl(drawitem->hDC, &r, DFC_BUTTON, DFCS_BUTTONCHECK | DFCS_HOT);
						else DrawFrameControl(drawitem->hDC, &r, DFC_BUTTON, DFCS_BUTTONCHECK);
					}
				}
				drawitem->rcItem.left += GetSystemMetrics(SM_CXMENUCHECK) + 5;
			}
			combotext[0] = 0;
			if (drawitem->itemID != -1 && !(drawitem->itemState & ODS_COMBOBOXEDIT))
				SendDlgItemMessage(hWnd, IDC_EXTRAMODES, CB_GETLBTEXT, drawitem->itemID, combotext);
			else
			{
				switch (cfg->AddModes)
				{
				case 0:
					_tcscpy(combotext, _T("None"));
					break;
				case 1:
					_tcscpy(combotext, extramodes[0]);
					break;
				case 2:
					_tcscpy(combotext, extramodes[1]);
					break;
				case 4:
					_tcscpy(combotext, extramodes[2]);
					break;
				case 8:
					_tcscpy(combotext, extramodes[3]);
					break;
				case 16:
					_tcscpy(combotext, extramodes[4]);
					break;
				case 32:
					_tcscpy(combotext, extramodes[5]);
					break;
				case 64:
					_tcscpy(combotext, extramodes[6]);
					break;
				default:
					_tcscpy(combotext, _T("Multiple selections"));
				}
			}
			DrawText(drawitem->hDC, combotext, _tcslen(combotext), &drawitem->rcItem,
				DT_LEFT | DT_SINGLELINE | DT_VCENTER);
			SetTextColor(drawitem->hDC, OldTextColor);
			SetBkColor(drawitem->hDC, OldBackColor);
			if (drawitem->itemID != -1 && !(drawitem->itemState & ODS_COMBOBOXEDIT))
				drawitem->rcItem.left -= GetSystemMetrics(SM_CXMENUCHECK) + 5;
			if (drawitem->itemState & ODS_FOCUS) DrawFocusRect(drawitem->hDC, &drawitem->rcItem);
			DefWindowProc(hWnd, Msg, wParam, lParam);
			break;
		default:
			break;
		}
	case WM_THEMECHANGED:
		if (uxtheme)
		{
			if (hThemeDisplay) _CloseThemeData(hThemeDisplay);
			_OpenThemeData(hWnd, L"Button");
		}
	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case IDC_COLORDEPTH:
			if (HIWORD(wParam) == CBN_SELENDOK)
			{
				if (ColorDepth_Dropdown)
				{
					cursel = SendDlgItemMessage(hWnd, IDC_COLORDEPTH, CB_GETCURSEL, 0, 0);
					i = ((cfg->AddColorDepths >> cursel) & 1);
					if (i) cfg->AddColorDepths &= ~(1 << cursel);
					else cfg->AddColorDepths |= 1 << cursel;
					EnableWindow(GetDlgItem(hDialog, IDC_APPLY), TRUE);
					*dirty = TRUE;
				}
			}
			if (HIWORD(wParam) == CBN_DROPDOWN)
			{
				ColorDepth_Dropdown = TRUE;
			}
			if (HIWORD(wParam) == CBN_CLOSEUP)
			{
				ColorDepth_Dropdown = FALSE;
			}
			break;
		case IDC_EXTRAMODES:
			if (HIWORD(wParam) == CBN_SELENDOK)
			{
				if (ExtraModes_Dropdown)
				{
					cursel = SendDlgItemMessage(hWnd, IDC_EXTRAMODES, CB_GETCURSEL, 0, 0);
					i = ((cfg->AddModes >> cursel) & 1);
					if (i) cfg->AddModes &= ~(1 << cursel);
					else cfg->AddModes |= 1 << cursel;
					EnableWindow(GetDlgItem(hDialog, IDC_APPLY), TRUE);
					*dirty = TRUE;
				}
			}
			if (HIWORD(wParam) == CBN_DROPDOWN)
			{
				ExtraModes_Dropdown = TRUE;
			}
			if (HIWORD(wParam) == CBN_CLOSEUP)
			{
				ExtraModes_Dropdown = FALSE;
			}
			break;
		case IDC_VIDMODE:
			cfg->scaler = GetCombo(hWnd, IDC_VIDMODE, &cfgmask->scaler);
			EnableWindow(GetDlgItem(hDialog, IDC_APPLY), TRUE);
			*dirty = TRUE;
			break;
		case IDC_SCALE:
			cfg->scalingfilter = GetCombo(hWnd, IDC_SCALE, &cfgmask->scalingfilter);
			EnableWindow(GetDlgItem(hDialog, IDC_APPLY), TRUE);
			*dirty = TRUE;
			break;
		case IDC_ASPECT:
			if (HIWORD(wParam) == CBN_KILLFOCUS)
			{
				cfg->aspect = GetAspectCombo(hWnd, IDC_ASPECT, &cfgmask->aspect);
				SetAspectCombo(hWnd, IDC_ASPECT, cfg->aspect, cfgmask->aspect, tristate);
				EnableWindow(GetDlgItem(hDialog, IDC_APPLY), TRUE);
				*dirty = TRUE;
			}
			else if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				cfg->aspect = GetAspectCombo(hWnd, IDC_ASPECT, &cfgmask->aspect);
				EnableWindow(GetDlgItem(hDialog, IDC_APPLY), TRUE);
				*dirty = TRUE;
			}
			break;
		case IDC_SORTMODES:
			cfg->SortModes = GetCombo(hWnd, IDC_SORTMODES, &cfgmask->SortModes);
			EnableWindow(GetDlgItem(hDialog, IDC_APPLY), TRUE);
			*dirty = TRUE;
			break;
		case IDC_DPISCALE:
			cfg->DPIScale = GetCombo(hWnd, IDC_DPISCALE, &cfgmask->DPIScale);
			EnableWindow(GetDlgItem(hDialog, IDC_APPLY), TRUE);
			*dirty = TRUE;
			break;
		case IDC_VSYNC:
			cfg->vsync = GetCombo(hWnd, IDC_VSYNC, &cfgmask->vsync);
			EnableWindow(GetDlgItem(hDialog, IDC_APPLY), TRUE);
			*dirty = TRUE;
			break;
		case IDC_FULLMODE:
			cfg->fullmode = GetCombo(hWnd, IDC_FULLMODE, &cfgmask->fullmode);
			EnableWindow(GetDlgItem(hDialog, IDC_APPLY), TRUE);
			*dirty = TRUE;
			break;
		case IDC_COLOR:
			cfg->colormode = GetCheck(hWnd, IDC_COLOR, &cfgmask->colormode);
			EnableWindow(GetDlgItem(hDialog, IDC_APPLY), TRUE);
			*dirty = TRUE;
			break;
		}
	}
	default:
		return FALSE;
	}
	return TRUE;
}
LRESULT CALLBACK Tab3DCallback(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
	case WM_INITDIALOG:
		if (_EnableThemeDialogTexture) _EnableThemeDialogTexture(hWnd, ETDT_ENABLETAB);
		return TRUE;
	default:
		return FALSE;
	}
	return TRUE;
}
LRESULT CALLBACK EffectsTabCallback(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
	case WM_INITDIALOG:
		if (_EnableThemeDialogTexture) _EnableThemeDialogTexture(hWnd, ETDT_ENABLETAB);
		return TRUE;
	default:
		return FALSE;
	}
	return TRUE;
}
LRESULT CALLBACK AdvancedTabCallback(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
	case WM_INITDIALOG:
		if (_EnableThemeDialogTexture) _EnableThemeDialogTexture(hWnd, ETDT_ENABLETAB);
		return TRUE;
	default:
		return FALSE;
	}
	return TRUE;
}
LRESULT CALLBACK DebugTabCallback(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
	case WM_INITDIALOG:
		if (_EnableThemeDialogTexture) _EnableThemeDialogTexture(hWnd, ETDT_ENABLETAB);
		return TRUE;
	default:
		return FALSE;
	}
	return TRUE;
}
LRESULT CALLBACK PathsTabCallback(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
	case WM_INITDIALOG:
		if (_EnableThemeDialogTexture) _EnableThemeDialogTexture(hWnd, ETDT_ENABLETAB);
		return TRUE;
	default:
		return FALSE;
	}
	return TRUE;
}
LRESULT CALLBACK DXGLCfgCallback(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	PIXELFORMATDESCRIPTOR pfd =
	    {
			sizeof(PIXELFORMATDESCRIPTOR),
			1,
			PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
			PFD_TYPE_RGBA,            //The kind of framebuffer. RGBA or palette.
			0,                        //Colordepth of the framebuffer.
			0, 0, 0, 0, 0, 0,
			0,
			0,
			0,
			0, 0, 0, 0,
			24,                        //Number of bits for the depthbuffer
			8,                        //Number of bits for the stencilbuffer
			0,                        //Number of Aux buffers in the framebuffer.
			PFD_MAIN_PLANE,
			0,
	        0, 0, 0
	    };
	GLfloat anisotropic;
	HDC dc;
	HGLRC rc;
	GLint maxsamples;
	GLint maxcoverage = 0;
	GLint coveragemodes[64];
	int msaamodes[32];
	int pf;
	int i;
	HKEY hKeyBase;
	HKEY hKey;
	DWORD keysize,keysize2;
	DEVMODE mode;
	LPTSTR keyname;
	LPTSTR regbuffer;
	DWORD regbuffersize;
	DWORD buffersize;
	LONG error;
	TCHAR buffer[64];
	TCHAR subkey[MAX_PATH];
	LPTSTR path;
	SHFILEINFO fileinfo;
	DWORD verinfosize;
	LPTSTR outbuffer;
	UINT outlen;
	TCHAR verpath[64];
	WORD translation[2];
	DWORD cursel;
	DRAWITEMSTRUCT* drawitem;
	BOOL hasname;
	void *verinfo;
	COLORREF OldTextColor,OldBackColor;
	HANDLE token = NULL;
	TOKEN_ELEVATION elevation;
	HWND hGLWnd;
	OPENFILENAME filename;
	TCHAR selectedfile[MAX_PATH + 1];
	LPTSTR regpath;
	LPTSTR regkey;
	BOOL failed;
	LPTSTR installpath;
	DWORD err;
	RECT r;
	NMHDR *nm;
	int newtab;
	TCITEM tab;
	switch (Msg)
	{
	case WM_INITDIALOG:
		hDialog = hWnd;
		tristate = FALSE;
		maxapps = 128;
		apps = (app_setting *)malloc(maxapps*sizeof(app_setting));
		apps[0].name = (TCHAR*)malloc(7 * sizeof(TCHAR));
		_tcscpy(apps[0].name,_T("Global"));
		apps[0].regkey = (TCHAR*)malloc(7 * sizeof(TCHAR));
		_tcscpy(apps[0].regkey,_T("Global"));
		GetGlobalConfig(&apps[0].cfg, FALSE);
		cfg = &apps[0].cfg;
		cfgmask = &apps[0].mask;
		dirty = &apps[0].dirty;
		memset(&apps[0].mask,0xff,sizeof(DXGLCFG));
		apps[0].dirty = FALSE;
		apps[0].icon = LoadIcon(GetModuleHandle(NULL),MAKEINTRESOURCE(IDI_STAR));
		apps[0].icon_shared = TRUE;
		apps[0].path[0] = 0;
		SetClassLong(hWnd,GCL_HICON,(LONG)LoadIcon(hinstance,(LPCTSTR)IDI_DXGL));
		SetClassLong(hWnd,GCL_HICONSM,(LONG)LoadIcon(hinstance,(LPCTSTR)IDI_DXGLSM));
		// create temporary gl context to get AA and AF settings.
		mode.dmSize = sizeof(DEVMODE);
		EnumDisplaySettings(NULL,ENUM_CURRENT_SETTINGS,&mode);
		pfd.cColorBits = (BYTE)mode.dmBitsPerPel;
		hGLWnd = CreateWindow(_T("STATIC"),NULL,WS_CHILD,0,0,16,16,hWnd,NULL,NULL,NULL);
		dc = GetDC(hGLWnd);
		pf = ChoosePixelFormat(dc,&pfd);
		SetPixelFormat(dc,pf,&pfd);
		rc = wglCreateContext(dc);
		wglMakeCurrent(dc,rc);
		extensions_string = (char*)glGetString(GL_EXTENSIONS);
		if(strstr(extensions_string,"GL_EXT_texture_filter_anisotropic"))
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&anisotropic);
		else anisotropic = 0;
		if(strstr(extensions_string,"GL_EXT_framebuffer_multisample"))
		{
			glGetIntegerv(GL_MAX_SAMPLES_EXT,&maxsamples);
			if(strstr(extensions_string,"GL_NV_framebuffer_multisample_coverage")) // Supports NVIDIA CSAA
			{
				glGetIntegerv(GL_MAX_MULTISAMPLE_COVERAGE_MODES_NV,&maxcoverage);
				glGetIntegerv(GL_MULTISAMPLE_COVERAGE_MODES_NV,coveragemodes);
				if(maxcoverage) for(i = 0; i < maxcoverage; i++)
				{
					msaamodes[i] = coveragemodes[2*i]+(4096*coveragemodes[(2*i)+1]);
					msaa = TRUE;
				}
			}
		}
		wglMakeCurrent(dc,NULL);
		wglDeleteContext(rc);
		ReleaseDC(hGLWnd,dc);
		DestroyWindow(hGLWnd);
		uxtheme = LoadLibrary(_T("uxtheme.dll"));
		if (uxtheme)
		{

			_OpenThemeData = (HTHEME(WINAPI*)(HWND,LPCWSTR))GetProcAddress(uxtheme, "OpenThemeData");
			_CloseThemeData = (HRESULT(WINAPI*)(HTHEME))GetProcAddress(uxtheme, "CloseThemeData");
			_DrawThemeBackground = 
				(HRESULT(WINAPI*)(HTHEME, HDC, int, int, const RECT*, const RECT*))
				GetProcAddress(uxtheme, "DrawThemeBackground");
			_EnableThemeDialogTexture = (HRESULT(WINAPI*)(HWND, DWORD))
				GetProcAddress(uxtheme, "EnableThemeDialogTexture");
			if (!(_OpenThemeData && _CloseThemeData && _DrawThemeBackground && _EnableThemeDialogTexture))
			{
				FreeLibrary(uxtheme);
				uxtheme = NULL;
			}
		}
		// Add tabs
		ZeroMemory(&tab, sizeof(TCITEM));
		tab.mask = TCIF_TEXT;
		tab.pszText = _T("Display");
		SendDlgItemMessage(hWnd, IDC_TABS, TCM_INSERTITEM, 0, (LPARAM)&tab);
		tab.pszText = _T("Effects");
		SendDlgItemMessage(hWnd, IDC_TABS, TCM_INSERTITEM, 1, (LPARAM)&tab);
		tab.pszText = _T("3D Graphics");
		SendDlgItemMessage(hWnd, IDC_TABS, TCM_INSERTITEM, 2, (LPARAM)&tab);
		tab.pszText = _T("Advanced");
		SendDlgItemMessage(hWnd, IDC_TABS, TCM_INSERTITEM, 3, (LPARAM)&tab);
		tab.pszText = _T("Debug");
		SendDlgItemMessage(hWnd, IDC_TABS, TCM_INSERTITEM, 4, (LPARAM)&tab);
		hTab = GetDlgItem(hWnd, IDC_TABS);
		hTabs[0] = CreateDialog(hinstance, MAKEINTRESOURCE(IDD_DISPLAY), hTab, DisplayTabCallback);
		hTabs[1] = CreateDialog(hinstance, MAKEINTRESOURCE(IDD_EFFECTS), hTab, EffectsTabCallback);
		hTabs[2] = CreateDialog(hinstance, MAKEINTRESOURCE(IDD_3DGRAPHICS), hTab, Tab3DCallback);
		hTabs[3] = CreateDialog(hinstance, MAKEINTRESOURCE(IDD_ADVANCED), hTab, AdvancedTabCallback);
		hTabs[4] = CreateDialog(hinstance, MAKEINTRESOURCE(IDD_DEBUG), hTab, DebugTabCallback);
		SendDlgItemMessage(hWnd, IDC_TABS, TCM_GETITEMRECT, 0, (LPARAM)&r);
		SetWindowPos(hTabs[0], NULL, r.left, r.bottom + 3, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
		ShowWindow(hTabs[1], SW_HIDE);
		ShowWindow(hTabs[2], SW_HIDE);
		ShowWindow(hTabs[3], SW_HIDE);
		ShowWindow(hTabs[4], SW_HIDE);
		ShowWindow(hTabs[5], SW_HIDE);
		tabopen = 0;

		// Load global settings.
		// video mode
		_tcscpy(buffer,_T("Change desktop resolution"));
		SendDlgItemMessage(hTabs[0], IDC_VIDMODE, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer,_T("Stretch to screen"));
		SendDlgItemMessage(hTabs[0],IDC_VIDMODE,CB_ADDSTRING,1,(LPARAM)buffer);
		_tcscpy(buffer,_T("Aspect corrected stretch"));
		SendDlgItemMessage(hTabs[0], IDC_VIDMODE, CB_ADDSTRING, 2, (LPARAM)buffer);
		_tcscpy(buffer,_T("Center image on screen"));
		SendDlgItemMessage(hTabs[0], IDC_VIDMODE, CB_ADDSTRING, 3, (LPARAM)buffer);
		_tcscpy(buffer,_T("Stretch if mode not found"));
		SendDlgItemMessage(hTabs[0], IDC_VIDMODE, CB_ADDSTRING, 4, (LPARAM)buffer);
		_tcscpy(buffer,_T("Scale if mode not found"));
		SendDlgItemMessage(hTabs[0], IDC_VIDMODE, CB_ADDSTRING, 5, (LPARAM)buffer);
		_tcscpy(buffer,_T("Center if mode not found"));
		SendDlgItemMessage(hTabs[0], IDC_VIDMODE, CB_ADDSTRING, 6, (LPARAM)buffer);
		_tcscpy(buffer,_T("Crop to screen (experimental)"));
		SendDlgItemMessage(hTabs[0],IDC_VIDMODE,CB_ADDSTRING,7,(LPARAM)buffer);
		SendDlgItemMessage(hTabs[0],IDC_VIDMODE,CB_SETCURSEL,cfg->scaler,0);
		// fullscreen window mode
		_tcscpy(buffer, _T("Exclusive fullscreen"));
		SendDlgItemMessage(hTabs[0], IDC_FULLMODE, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer, _T("Non-exclusive fullscreen"));
		SendDlgItemMessage(hTabs[0], IDC_FULLMODE, CB_ADDSTRING, 1, (LPARAM)buffer);
		_tcscpy(buffer, _T("Non-resizable window"));
		SendDlgItemMessage(hTabs[0], IDC_FULLMODE, CB_ADDSTRING, 2, (LPARAM)buffer);
		_tcscpy(buffer, _T("Resizable window"));
		SendDlgItemMessage(hTabs[0], IDC_FULLMODE, CB_ADDSTRING, 3, (LPARAM)buffer);
		_tcscpy(buffer, _T("Borderless window"));
		SendDlgItemMessage(hTabs[0], IDC_FULLMODE, CB_ADDSTRING, 4, (LPARAM)buffer);
		_tcscpy(buffer, _T("Borderless window (scaled)"));
		SendDlgItemMessage(hTabs[0], IDC_FULLMODE, CB_ADDSTRING, 5, (LPARAM)buffer);
		SendDlgItemMessage(hTabs[0], IDC_FULLMODE, CB_SETCURSEL, cfg->fullmode, 0);
		// colormode
		if (cfg->colormode) SendDlgItemMessage(hTabs[0], IDC_COLOR, BM_SETCHECK, BST_CHECKED, 0);
		else SendDlgItemMessage(hTabs[0], IDC_COLOR, BM_SETCHECK, BST_UNCHECKED, 0);
		// first scaling filter
		_tcscpy(buffer, _T("Nearest"));
		SendDlgItemMessage(hTabs[2], IDC_POSTSCALE, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer, _T("Bilinear"));
		SendDlgItemMessage(hTabs[2], IDC_POSTSCALE, CB_ADDSTRING, 1, (LPARAM)buffer);
		SendDlgItemMessage(hTabs[2], IDC_POSTSCALE, CB_SETCURSEL, cfg->postfilter, 0);
		// first scaling sizes
		_tcscpy(buffer, _T("Auto"));
		SendDlgItemMessage(hTabs[2], IDC_POSTSCALESIZE, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer, _T("1x"));
		SendDlgItemMessage(hTabs[2], IDC_POSTSCALESIZE, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer, _T("2x1"));
		SendDlgItemMessage(hTabs[2], IDC_POSTSCALESIZE, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer, _T("2x"));
		SendDlgItemMessage(hTabs[2], IDC_POSTSCALESIZE, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer, _T("3x"));
		SendDlgItemMessage(hTabs[2], IDC_POSTSCALESIZE, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer, _T("4x"));
		SendDlgItemMessage(hTabs[2], IDC_POSTSCALESIZE, CB_ADDSTRING, 0, (LPARAM)buffer);
		SetPostScaleCombo(hTabs[2], IDC_POSTSCALESIZE, cfg->postsizex, cfg->postsizey,
			cfgmask->postsizex, cfgmask->postsizey, tristate);
		// final scaling filter
		_tcscpy(buffer,_T("Nearest"));
		SendDlgItemMessage(hTabs[0], IDC_SCALE, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer,_T("Bilinear"));
		SendDlgItemMessage(hTabs[0],IDC_SCALE,CB_ADDSTRING,1,(LPARAM)buffer);
		SendDlgItemMessage(hTabs[0],IDC_SCALE,CB_SETCURSEL,cfg->scalingfilter,0);
		// aspect
		_tcscpy(buffer,_T("Default"));
		SendDlgItemMessage(hTabs[0], IDC_ASPECT, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer,_T("4:3"));
		SendDlgItemMessage(hTabs[0], IDC_ASPECT, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer,_T("16:10"));
		SendDlgItemMessage(hTabs[0], IDC_ASPECT, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer,_T("16:9"));
		SendDlgItemMessage(hTabs[0], IDC_ASPECT, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer,_T("5:4"));
		SendDlgItemMessage(hTabs[0], IDC_ASPECT, CB_ADDSTRING, 0, (LPARAM)buffer);
		SetAspectCombo(hTabs[0], IDC_ASPECT, cfg->aspect, cfgmask->aspect, tristate);
		// primaryscale
		_tcscpy(buffer, _T("Auto (Window Size)"));
		SendDlgItemMessage(hTabs[2], IDC_PRIMARYSCALE, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer, _T("Auto (Multiple of Native)"));
		SendDlgItemMessage(hTabs[2], IDC_PRIMARYSCALE, CB_ADDSTRING, 1, (LPARAM)buffer);
		_tcscpy(buffer, _T("1x Native (Recommended)"));
		SendDlgItemMessage(hTabs[2], IDC_PRIMARYSCALE, CB_ADDSTRING, 2, (LPARAM)buffer);
		_tcscpy(buffer, _T("1.5x Native"));
		SendDlgItemMessage(hTabs[2], IDC_PRIMARYSCALE, CB_ADDSTRING, 3, (LPARAM)buffer);
		_tcscpy(buffer, _T("2x Native"));
		SendDlgItemMessage(hTabs[2], IDC_PRIMARYSCALE, CB_ADDSTRING, 4, (LPARAM)buffer);
		_tcscpy(buffer, _T("2.5x Native"));
		SendDlgItemMessage(hTabs[2], IDC_PRIMARYSCALE, CB_ADDSTRING, 5, (LPARAM)buffer);
		_tcscpy(buffer, _T("3x Native"));
		SendDlgItemMessage(hTabs[2], IDC_PRIMARYSCALE, CB_ADDSTRING, 6, (LPARAM)buffer);
		_tcscpy(buffer, _T("4x Native"));
		SendDlgItemMessage(hTabs[2], IDC_PRIMARYSCALE, CB_ADDSTRING, 7, (LPARAM)buffer);
		_tcscpy(buffer, _T("Custom"));
		SendDlgItemMessage(hTabs[2], IDC_PRIMARYSCALE, CB_ADDSTRING, 8, (LPARAM)buffer);
		SetPrimaryScaleCombo(hTabs[2], IDC_PRIMARYSCALE, cfg->primaryscale, cfgmask->primaryscale, tristate);
		// texfilter
		_tcscpy(buffer,_T("Application default"));
		SendDlgItemMessage(hTabs[2], IDC_TEXFILTER, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer,_T("Nearest"));
		SendDlgItemMessage(hTabs[2], IDC_TEXFILTER, CB_ADDSTRING, 1, (LPARAM)buffer);
		_tcscpy(buffer,_T("Bilinear"));
		SendDlgItemMessage(hTabs[2], IDC_TEXFILTER, CB_ADDSTRING, 2, (LPARAM)buffer);
		_tcscpy(buffer,_T("Nearest, nearest mipmap"));
		SendDlgItemMessage(hTabs[2], IDC_TEXFILTER, CB_ADDSTRING, 3, (LPARAM)buffer);
		_tcscpy(buffer,_T("Nearest, linear mipmap"));
		SendDlgItemMessage(hTabs[2], IDC_TEXFILTER, CB_ADDSTRING, 4, (LPARAM)buffer);
		_tcscpy(buffer,_T("Bilinear, nearest mipmap"));
		SendDlgItemMessage(hTabs[2], IDC_TEXFILTER, CB_ADDSTRING, 5, (LPARAM)buffer);
		_tcscpy(buffer,_T("Bilinear, linear mipmap"));
		SendDlgItemMessage(hTabs[2],IDC_TEXFILTER,CB_ADDSTRING,6,(LPARAM)buffer);
		SendDlgItemMessage(hTabs[2],IDC_TEXFILTER,CB_SETCURSEL,cfg->texfilter,0);
		// anisotropic
		if (anisotropic < 2)
		{
			_tcscpy(buffer,_T("Not supported"));
			SendDlgItemMessage(hTabs[2], IDC_ANISO, CB_ADDSTRING, 0, (LPARAM)buffer);
			SendDlgItemMessage(hTabs[2], IDC_ANISO, CB_SETCURSEL, 0, 0);
			EnableWindow(GetDlgItem(hTabs[2], IDC_ANISO), FALSE);
			cfg->anisotropic = 0;
		}
		else
		{
			_tcscpy(buffer,_T("Application default"));
			SendDlgItemMessage(hTabs[2], IDC_ANISO, CB_ADDSTRING, 0, (LPARAM)buffer);
			_tcscpy(buffer,_T("Disabled"));
			SendDlgItemMessage(hTabs[2], IDC_ANISO, CB_ADDSTRING, 1, (LPARAM)buffer);
			if(anisotropic >= 2)
			{
				_tcscpy(buffer,_T("2x"));
				SendDlgItemMessage(hTabs[2], IDC_ANISO, CB_ADDSTRING, 2, (LPARAM)buffer);
			}
			if(anisotropic >= 4)
			{
				_tcscpy(buffer,_T("4x"));
				SendDlgItemMessage(hTabs[2], IDC_ANISO, CB_ADDSTRING, 4, (LPARAM)buffer);
			}
			if(anisotropic >= 8)
			{
				_tcscpy(buffer,_T("8x"));
				SendDlgItemMessage(hTabs[2], IDC_ANISO, CB_ADDSTRING, 8, (LPARAM)buffer);
			}
			if(anisotropic >= 16)
			{
				_tcscpy(buffer,_T("16x"));
				SendDlgItemMessage(hTabs[2], IDC_ANISO, CB_ADDSTRING, 16, (LPARAM)buffer);
			}
			if(anisotropic >= 32)
			{
				_tcscpy(buffer,_T("32x"));
				SendDlgItemMessage(hTabs[2], IDC_ANISO, CB_ADDSTRING, 4, (LPARAM)buffer);
			}
			SendDlgItemMessage(hTabs[2], IDC_ANISO, CB_SETCURSEL, cfg->anisotropic, 0);
		}
		// msaa
		if(msaa)
		{
			_tcscpy(buffer,_T("Application default"));
			SendDlgItemMessage(hTabs[2], IDC_MSAA, CB_ADDSTRING, 0, (LPARAM)buffer);
			_tcscpy(buffer,_T("Disabled"));
			SendDlgItemMessage(hTabs[2], IDC_MSAA, CB_ADDSTRING, 1, (LPARAM)buffer);
			if(maxcoverage)
			{
				for(i = 0; i < maxcoverage; i++)
				{
					if((msaamodes[i] & 0xfff) <= 4)
						_sntprintf(buffer,64,_T("%dx"),msaamodes[i] & 0xfff);
					else _sntprintf(buffer,64,_T("%dx coverage, %dx color"),(msaamodes[i] & 0xfff), (msaamodes[i] >> 12));
					SendDlgItemMessage(hTabs[2], IDC_MSAA, CB_ADDSTRING, msaamodes[i], (LPARAM)buffer);
				}
			}
			else
			{
				if(maxsamples >= 2)
				{
					_tcscpy(buffer,_T("2x"));
					SendDlgItemMessage(hTabs[2], IDC_MSAA, CB_ADDSTRING, 2, (LPARAM)buffer);
				}
				if(maxsamples >= 4)
				{
					_tcscpy(buffer,_T("4x"));
					SendDlgItemMessage(hTabs[2], IDC_MSAA, CB_ADDSTRING, 4, (LPARAM)buffer);
				}
				if(maxsamples >= 8)
				{
					_tcscpy(buffer,_T("8x"));
					SendDlgItemMessage(hTabs[2], IDC_MSAA, CB_ADDSTRING, 8, (LPARAM)buffer);
				}
				if(maxsamples >= 16)
				{
					_tcscpy(buffer,_T("16x"));
					SendDlgItemMessage(hTabs[2], IDC_MSAA, CB_ADDSTRING, 16, (LPARAM)buffer);
				}
				if(maxsamples >= 32)
				{
					_tcscpy(buffer,_T("32x"));
					SendDlgItemMessage(hTabs[2], IDC_MSAA, CB_ADDSTRING, 32, (LPARAM)buffer);
				}
			}
			SendDlgItemMessage(hTabs[2], IDC_MSAA, CB_SETCURSEL, cfg->msaa, 0);
		}
		else
		{
			_tcscpy(buffer,_T("Not supported"));
			SendDlgItemMessage(hTabs[2], IDC_MSAA, CB_ADDSTRING, 0, (LPARAM)buffer);
			SendDlgItemMessage(hTabs[2], IDC_MSAA, CB_SETCURSEL, 0, 0);
			EnableWindow(GetDlgItem(hTabs[2], IDC_MSAA), FALSE);
			cfg->msaa = 0;
		}
		// aspect3d
		_tcscpy(buffer,_T("Stretch to display"));
		SendDlgItemMessage(hTabs[2], IDC_ASPECT3D, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer,_T("Expand viewable area"));
		SendDlgItemMessage(hTabs[2], IDC_ASPECT3D, CB_ADDSTRING, 1, (LPARAM)buffer);
		_tcscpy(buffer,_T("Crop to display"));
		SendDlgItemMessage(hTabs[2],IDC_ASPECT3D,CB_ADDSTRING,2,(LPARAM)buffer);
		SendDlgItemMessage(hTabs[2],IDC_ASPECT3D,CB_SETCURSEL,cfg->aspect3d,0);
		// sort modes
		_tcscpy(buffer,_T("Use system order"));
		SendDlgItemMessage(hTabs[0], IDC_SORTMODES, CB_ADDSTRING, 0, (LPARAM)buffer);
		_tcscpy(buffer,_T("Group by color depth"));
		SendDlgItemMessage(hTabs[0], IDC_SORTMODES, CB_ADDSTRING, 1, (LPARAM)buffer);
		_tcscpy(buffer,_T("Group by resolution"));
		SendDlgItemMessage(hTabs[0],IDC_SORTMODES,CB_ADDSTRING,2,(LPARAM)buffer);
		SendDlgItemMessage(hTabs[0],IDC_SORTMODES,CB_SETCURSEL,cfg->SortModes,0);
		// color depths
		for (i = 0; i < 5; i++)
		{
			_tcscpy(buffer, colormodedropdown[i]);
			SendDlgItemMessage(hTabs[0], IDC_COLORDEPTH, CB_ADDSTRING, i, (LPARAM)buffer);
		}
		SendDlgItemMessage(hTabs[0], IDC_COLORDEPTH, CB_SETCURSEL, cfg->AddColorDepths, 0);
		for (i = 0; i < 7; i++)
		{
			_tcscpy(buffer, extramodes[i]);
			SendDlgItemMessage(hTabs[0], IDC_EXTRAMODES, CB_ADDSTRING, i, (LPARAM)buffer);
		}
		// Enable shader
		if (cfg->colormode) SendDlgItemMessage(hTabs[1], IDC_USESHADER, BM_SETCHECK, BST_CHECKED, 0);
		else SendDlgItemMessage(hTabs[1], IDC_USESHADER, BM_SETCHECK, BST_UNCHECKED, 0);
		// shader path
		SetText(hTabs[1],IDC_SHADER,cfg->shaderfile,cfgmask->shaderfile,FALSE);
		// texture format
		_tcscpy(buffer,_T("Automatic"));
		SendDlgItemMessage(hTabs[3],IDC_TEXTUREFORMAT,CB_ADDSTRING,0,(LPARAM)buffer);
		SendDlgItemMessage(hTabs[3],IDC_TEXTUREFORMAT,CB_SETCURSEL,cfg->TextureFormat,0);
		// Texture upload
		_tcscpy(buffer,_T("Automatic"));
		SendDlgItemMessage(hTabs[3],IDC_TEXUPLOAD,CB_ADDSTRING,0,(LPARAM)buffer);
		SendDlgItemMessage(hTabs[3],IDC_TEXUPLOAD,CB_SETCURSEL,cfg->TexUpload,0);
		// DPI
		_tcscpy(buffer, _T("Disabled"));
		SendDlgItemMessage(hTabs[0],IDC_DPISCALE,CB_ADDSTRING,0,(LPARAM)buffer);
		_tcscpy(buffer, _T("Enabled"));
		SendDlgItemMessage(hTabs[0],IDC_DPISCALE,CB_ADDSTRING,1,(LPARAM)buffer);
		_tcscpy(buffer, _T("Windows AppCompat"));
		SendDlgItemMessage(hTabs[0],IDC_DPISCALE,CB_ADDSTRING,2,(LPARAM)buffer);
		SendDlgItemMessage(hTabs[0],IDC_DPISCALE,CB_SETCURSEL,cfg->DPIScale,0);
		EnableWindow(GetDlgItem(hTabs[3], IDC_PATHLABEL), FALSE);
		EnableWindow(GetDlgItem(hTabs[3], IDC_PROFILEPATH), FALSE);
		// Check install path
		installpath = NULL;
		error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\DXGL"), 0, KEY_READ, &hKey);
		if (error == ERROR_SUCCESS)
		{
			if (RegQueryValueEx(hKey, _T("InstallDir"), NULL, NULL, NULL, &keysize) == ERROR_SUCCESS)
			{
				installpath = (LPTSTR)malloc(keysize);
				error = RegQueryValueEx(hKey, _T("InstallDir"), NULL, NULL, installpath, &keysize);
				if (error != ERROR_SUCCESS)
				{
					free(installpath);
					installpath = NULL;
				}
			}
			RegCloseKey(hKey);
		}
		hKey = NULL;
		// Add installed programs
		current_app = 1;
		appcount = 1;
		regbuffersize = 1024;
		regbuffer = (LPTSTR)malloc(regbuffersize * sizeof(TCHAR));
		RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\DXGL\\Profiles"), 0, NULL, 0, KEY_READ, NULL, &hKeyBase, NULL);
		RegQueryInfoKey(hKeyBase, NULL, NULL, NULL, NULL, &keysize, NULL, NULL, NULL, NULL, NULL, NULL);
		keysize++;
		keyname = (LPTSTR)malloc(keysize * sizeof(TCHAR));
		keysize2 = keysize;
		i = 0;
		while (RegEnumKeyEx(hKeyBase, i, keyname, &keysize2, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			keysize2 = keysize;
			i++;
			appcount++;
			if (appcount > maxapps)
			{
				maxapps += 128;
				apps = (app_setting *)realloc(apps, maxapps * sizeof(app_setting));
			}
			_tcscpy(subkey, keyname);
			if (_tcsrchr(subkey, _T('-'))) *(_tcsrchr(subkey, _T('-'))) = 0;
			error = RegOpenKeyEx(hKeyBase, keyname, 0, KEY_READ, &hKey);
			buffersize = regbuffersize;
			RegQueryValueEx(hKey, _T("InstallPath"), NULL, NULL, NULL, &buffersize);
			if (buffersize > regbuffersize)
			{
				regbuffersize = buffersize;
				regbuffer = (LPTSTR)realloc(regbuffer, regbuffersize);
			}
			buffersize = regbuffersize;
			regbuffer[0] = 0;
			error = RegQueryValueEx(hKey, _T("InstallPath"), NULL, NULL, (LPBYTE)regbuffer, &buffersize);
			apps[appcount - 1].regkey = (LPTSTR)malloc((_tcslen(keyname) + 1) * sizeof(TCHAR));
			_tcscpy(apps[appcount - 1].regkey, keyname);
			GetConfig(&apps[appcount - 1].cfg, &apps[appcount - 1].mask, keyname);
			apps[appcount - 1].dirty = FALSE;
			if ((regbuffer[0] == 0) || error != ERROR_SUCCESS)
			{
				// Default icon
				apps[appcount - 1].icon = LoadIcon(NULL, IDI_APPLICATION);
				apps[appcount - 1].icon_shared = TRUE;
				apps[appcount - 1].name = (TCHAR*)malloc((_tcslen(subkey) + 1) * sizeof(TCHAR));
				_tcscpy(apps[appcount - 1].name, subkey);
				break;
			}
			path = (LPTSTR)malloc(((_tcslen(regbuffer) + _tcslen(subkey) + 2)) * sizeof(TCHAR));
			_tcscpy(path, regbuffer);
			_tcscpy(apps[appcount - 1].path, path);
			if (installpath)
			{
				if (!_tcsicmp(installpath, path)) apps[appcount - 1].builtin = TRUE;
				else apps[appcount - 1].builtin = FALSE;
			}
			_tcscat(path, _T("\\"));
			_tcscat(path, subkey);
			if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES)
			{
				// Default icon
				apps[appcount - 1].icon = LoadIcon(NULL, IDI_APPLICATION);
				apps[appcount - 1].icon_shared = TRUE;
				apps[appcount - 1].name = (TCHAR*)malloc((_tcslen(subkey) + 1) * sizeof(TCHAR));
				_tcscpy(apps[appcount - 1].name, subkey);
				break;
			}
			// Get exe attributes
			error = SHGetFileInfo(path, 0, &fileinfo, sizeof(SHFILEINFO), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_ADDOVERLAYS);
			apps[appcount - 1].icon = fileinfo.hIcon;
			apps[appcount - 1].icon_shared = FALSE;
			verinfosize = GetFileVersionInfoSize(path, NULL);
			verinfo = malloc(verinfosize);
			hasname = FALSE;
			if (GetFileVersionInfo(path, 0, verinfosize, verinfo))
			{
				if (VerQueryValue(verinfo, _T("\\VarFileInfo\\Translation"), (LPVOID*)&outbuffer, &outlen))
				{
					memcpy(translation, outbuffer, 4);
					_sntprintf(verpath, 64, _T("\\StringFileInfo\\%04x%04x\\FileDescription"), translation[0], translation[1]);
					if (VerQueryValue(verinfo, verpath, (LPVOID*)&outbuffer, &outlen))
					{
						hasname = TRUE;
						apps[appcount - 1].name = (LPTSTR)malloc((_tcslen(outbuffer) + 1) * sizeof(TCHAR));
						_tcscpy(apps[appcount - 1].name, outbuffer);
					}
					else
					{
						_sntprintf(verpath, 64, _T("\\StringFileInfo\\%04x%04x\\ProductName"), ((WORD*)outbuffer)[0], ((WORD*)outbuffer)[1]);
						if (VerQueryValue(verinfo, verpath, (LPVOID*)&outbuffer, &outlen))
						{
							hasname = TRUE;
							apps[appcount - 1].name = (LPTSTR)malloc((_tcslen(outbuffer) + 1) * sizeof(TCHAR));
							_tcscpy(apps[appcount - 1].name, outbuffer);
						}
						else
						{
							_sntprintf(verpath, 64, _T("\\StringFileInfo\\%04x%04x\\InternalName"), ((WORD*)outbuffer)[0], ((WORD*)outbuffer)[1]);
							if (VerQueryValue(verinfo, verpath, (LPVOID*)&outbuffer, &outlen))
							{
								hasname = TRUE;
								apps[appcount - 1].name = (LPTSTR)malloc((_tcslen(outbuffer) + 1) * sizeof(TCHAR));
								_tcscpy(apps[appcount - 1].name, outbuffer);
							}
						}
					}
				}
			}
			free(path);
			if (!hasname)
			{
				apps[appcount - 1].name = (LPTSTR)malloc((_tcslen(subkey) + 1) * sizeof(TCHAR));
				_tcscpy(apps[appcount - 1].name, subkey);
			}
			free(verinfo);
			RegCloseKey(hKey);
		}
		RegCloseKey(hKeyBase);
		free(keyname);
		for(i = 0; i < appcount; i++)
		{
			SendDlgItemMessage(hWnd,IDC_APPS,CB_ADDSTRING,0,(LPARAM)apps[i].name);
		}
		current_app = 0;
		SendDlgItemMessage(hWnd,IDC_APPS,CB_SETCURSEL,0,0);
		GetWindowRect(GetDlgItem(hWnd, IDC_APPS), &r);
		SetWindowPos(GetDlgItem(hWnd, IDC_APPS), HWND_TOP, r.left, r.top, r.right - r.left,
			(r.bottom - r.top) + (16 * (GetSystemMetrics(SM_CYSMICON) + 1)+(2*GetSystemMetrics(SM_CYBORDER))),
			SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
		if(osver.dwMajorVersion >= 6)
		{
			if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))
			{
				if(GetTokenInformation(token,(TOKEN_INFORMATION_CLASS)TokenElevation,&elevation,sizeof(TOKEN_ELEVATION),(PDWORD)&outlen))
				{
					if(!elevation.TokenIsElevated)
					{
						SendDlgItemMessage(hWnd,IDC_ADD,BCM_SETSHIELD,0,TRUE);
						SendDlgItemMessage(hWnd,IDC_REMOVE,BCM_SETSHIELD,0,TRUE);
					}
				}
			}
		}
		else
		{
			EnableWindow(GetDlgItem(hTabs[0], IDC_DPISCALE), FALSE);
		}
		if(token) CloseHandle(token);
		return TRUE;
	case WM_MEASUREITEM:
		switch(wParam)
		{
		case IDC_APPS:
			((LPMEASUREITEMSTRUCT)lParam)->itemHeight = GetSystemMetrics(SM_CYSMICON) + 1;
			((LPMEASUREITEMSTRUCT)lParam)->itemWidth = GetSystemMetrics(SM_CXSMICON)+1;
			break;
		default:
			break;
		}
		break;
	case WM_NOTIFY:
		nm = (LPNMHDR)lParam;
		if (nm->code == TCN_SELCHANGE)
		{
			newtab = SendDlgItemMessage(hWnd, IDC_TABS, TCM_GETCURSEL, 0, 0);
			if (newtab != tabopen)
			{
				ShowWindow(hTabs[tabopen], SW_HIDE);
				tabopen = newtab;
				SendDlgItemMessage(hWnd, IDC_TABS, TCM_GETITEMRECT, 0, (LPARAM)&r);
				SetWindowPos(hTabs[tabopen], NULL, r.left, r.bottom + 3, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
			}
		}
		break;
	case WM_DRAWITEM:
		drawitem = (DRAWITEMSTRUCT*)lParam;
		switch (wParam)
		{
		case IDC_APPS:
			OldTextColor = GetTextColor(drawitem->hDC);
			OldBackColor = GetBkColor(drawitem->hDC);
			if((drawitem->itemState & ODS_SELECTED))
			{
				SetTextColor(drawitem->hDC,GetSysColor(COLOR_HIGHLIGHTTEXT));
				SetBkColor(drawitem->hDC,GetSysColor(COLOR_HIGHLIGHT));
				FillRect(drawitem->hDC,&drawitem->rcItem,(HBRUSH)(COLOR_HIGHLIGHT+1));
			}
			else
			{
				SetTextColor(drawitem->hDC, GetSysColor(COLOR_WINDOWTEXT));
				SetBkColor(drawitem->hDC, GetSysColor(COLOR_WINDOW));
				FillRect(drawitem->hDC, &drawitem->rcItem, (HBRUSH)(COLOR_WINDOW + 1));
			}
			DrawIconEx(drawitem->hDC,drawitem->rcItem.left+2,drawitem->rcItem.top,
				apps[drawitem->itemID].icon,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),0,NULL,DI_NORMAL);
			drawitem->rcItem.left += GetSystemMetrics(SM_CXSMICON)+5;
			DrawText(drawitem->hDC,apps[drawitem->itemID].name,
				_tcslen(apps[drawitem->itemID].name),&drawitem->rcItem,
				DT_LEFT|DT_SINGLELINE|DT_VCENTER);
			drawitem->rcItem.left -= GetSystemMetrics(SM_CXSMICON)+5;
			if (drawitem->itemState & ODS_FOCUS) DrawFocusRect(drawitem->hDC, &drawitem->rcItem);
			SetTextColor(drawitem->hDC,OldTextColor);
			SetBkColor(drawitem->hDC,OldBackColor);
			DefWindowProc(hWnd,Msg,wParam,lParam);
			break;
		default:
			break;
		}
		break;
	case WM_HELP:
		HtmlHelp(hWnd,hlppath,HH_DISPLAY_TOPIC,(DWORD_PTR)_T("configuration.htm"));
		return TRUE;
		break;
	case WM_SYSCOMMAND:
		if(LOWORD(wParam) == SC_CONTEXTHELP)
		{
			HtmlHelp(hWnd,hlppath,HH_DISPLAY_TOPIC,(DWORD_PTR)_T("configuration.htm"));
			return TRUE;
		}
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			SaveChanges(hWnd);
			EndDialog(hWnd,IDOK);
			return TRUE;
		case IDCANCEL:
			EndDialog(hWnd,IDCANCEL);
			return TRUE;
		case IDC_APPLY:
			SaveChanges(hWnd);
			return TRUE;
		case IDC_APPS:
			if(HIWORD(wParam) == CBN_SELCHANGE)
			{
				cursel = SendDlgItemMessage(hWnd,IDC_APPS,CB_GETCURSEL,0,0);
				if(cursel == current_app) break;
				current_app = cursel;
				cfg = &apps[current_app].cfg;
				cfgmask = &apps[current_app].mask;
				dirty = &apps[current_app].dirty;
				if (current_app)
				{
					EnableWindow(GetDlgItem(hTabs[3], IDC_PATHLABEL), TRUE);
					EnableWindow(GetDlgItem(hTabs[3], IDC_PROFILEPATH), TRUE);
					SetDlgItemText(hTabs[3], IDC_PROFILEPATH, apps[current_app].path);
					if (apps[current_app].builtin) EnableWindow(GetDlgItem(hWnd, IDC_REMOVE), FALSE);
					else EnableWindow(GetDlgItem(hWnd, IDC_REMOVE), TRUE);
				}
				else
				{
					EnableWindow(GetDlgItem(hTabs[3], IDC_PATHLABEL), FALSE);
					EnableWindow(GetDlgItem(hTabs[3], IDC_PROFILEPATH), FALSE);
					SetDlgItemText(hTabs[3], IDC_PROFILEPATH, _T(""));
					EnableWindow(GetDlgItem(hWnd, IDC_REMOVE), FALSE);
				}
				// Set 3-state status
				if(current_app && !tristate)
				{
					tristate = TRUE;
					// Display tab
					SendDlgItemMessage(hTabs[0], IDC_VIDMODE, CB_ADDSTRING, 0, (LPARAM)strdefault);
					SendDlgItemMessage(hTabs[0], IDC_COLORDEPTH, CB_ADDSTRING, 0, (LPARAM)strdefault);
					SendDlgItemMessage(hTabs[0], IDC_SCALE, CB_ADDSTRING, 0, (LPARAM)strdefault);
					SendDlgItemMessage(hTabs[0], IDC_EXTRAMODES, CB_ADDSTRING, 0, (LPARAM)strdefault);
					SendDlgItemMessage(hTabs[0], IDC_ASPECT, CB_ADDSTRING, 0, (LPARAM)strdefault);
					SendDlgItemMessage(hTabs[0], IDC_SORTMODES, CB_ADDSTRING, 0, (LPARAM)strdefault);
					SendDlgItemMessage(hTabs[0], IDC_DPISCALE, CB_ADDSTRING, 0, (LPARAM)strdefault);
					SendDlgItemMessage(hTabs[0], IDC_VSYNC, CB_ADDSTRING, 0, (LPARAM)strdefault);
					SendDlgItemMessage(hTabs[0], IDC_FULLMODE, CB_ADDSTRING, 0, (LPARAM)strdefault);
					SendDlgItemMessage(hTabs[0], IDC_COLOR, BM_SETSTYLE, BS_AUTO3STATE, (LPARAM)TRUE);
					/*
					SendDlgItemMessage(hWnd, IDC_POSTSCALE, CB_ADDSTRING, 0, (LPARAM)strdefault);
					SendDlgItemMessage(hWnd, IDC_POSTSCALESIZE, CB_ADDSTRING, 0, (LPARAM)strdefault);
					SendDlgItemMessage(hWnd,IDC_MSAA,CB_ADDSTRING,0,(LPARAM)strdefault);
					SendDlgItemMessage(hWnd,IDC_ANISO,CB_ADDSTRING,0,(LPARAM)strdefault);
					SendDlgItemMessage(hWnd,IDC_TEXFILTER,CB_ADDSTRING,0,(LPARAM)strdefault);
					SendDlgItemMessage(hWnd,IDC_ASPECT3D,CB_ADDSTRING,0,(LPARAM)strdefault);
					SendDlgItemMessage(hTabs[0], IDC_HIGHRES, BM_SETSTYLE, BS_AUTO3STATE, (LPARAM)TRUE);
					SendDlgItemMessage(hWnd,IDC_TEXTUREFORMAT,CB_ADDSTRING,0,(LPARAM)strdefault);
					SendDlgItemMessage(hWnd,IDC_TEXUPLOAD,CB_ADDSTRING,0,(LPARAM)strdefault);
					*/
				}
				else if(!current_app && tristate)
				{
					tristate = FALSE;
					// Display tab
					SendDlgItemMessage(hTabs[0], IDC_VIDMODE, CB_DELETESTRING,
						SendDlgItemMessage(hTabs[0], IDC_VIDMODE, CB_FINDSTRING, -1, (LPARAM)strdefault), 0);
					SendDlgItemMessage(hTabs[0], IDC_COLORDEPTH, CB_DELETESTRING,
						SendDlgItemMessage(hTabs[0], IDC_COLORDEPTH, CB_FINDSTRING, -1, (LPARAM)strdefault), 0);
					SendDlgItemMessage(hTabs[0], IDC_SCALE, CB_DELETESTRING,
						SendDlgItemMessage(hTabs[0], IDC_SCALE, CB_FINDSTRING, -1, (LPARAM)strdefault), 0);
					SendDlgItemMessage(hTabs[0], IDC_EXTRAMODES, CB_DELETESTRING,
						SendDlgItemMessage(hTabs[0], IDC_EXTRAMODES, CB_FINDSTRING, -1, (LPARAM)strdefault), 0);
					SendDlgItemMessage(hTabs[0], IDC_ASPECT, CB_DELETESTRING,
						SendDlgItemMessage(hTabs[0], IDC_ASPECT, CB_FINDSTRING, -1, (LPARAM)strdefault), 0);
					SendDlgItemMessage(hTabs[0], IDC_SORTMODES, CB_DELETESTRING,
						SendDlgItemMessage(hTabs[0], IDC_SORTMODES, CB_FINDSTRING, -1, (LPARAM)strdefault), 0);
					SendDlgItemMessage(hTabs[0], IDC_DPISCALE, CB_DELETESTRING,
						SendDlgItemMessage(hTabs[0], IDC_DPISCALE, CB_FINDSTRING, -1, (LPARAM)strdefault), 0);
					SendDlgItemMessage(hTabs[0], IDC_VSYNC, CB_DELETESTRING,
						SendDlgItemMessage(hTabs[0], IDC_VSYNC, CB_FINDSTRING, -1, (LPARAM)strdefault), 0);
					SendDlgItemMessage(hTabs[0], IDC_FULLMODE, CB_DELETESTRING,
						SendDlgItemMessage(hTabs[0], IDC_FULLMODE, CB_FINDSTRING, -1, (LPARAM)strdefault), 0);
					SendDlgItemMessage(hTabs[0], IDC_COLOR, BM_SETSTYLE, BS_AUTOCHECKBOX, (LPARAM)TRUE);
					/*
					SendDlgItemMessage(hWnd, IDC_POSTSCALE, CB_DELETESTRING,
						SendDlgItemMessage(hWnd, IDC_POSTSCALE, CB_FINDSTRING, -1, (LPARAM)strdefault), 0);
					SendDlgItemMessage(hWnd, IDC_POSTSCALESIZE, CB_DELETESTRING,
						SendDlgItemMessage(hWnd, IDC_POSTSCALESIZE, CB_FINDSTRING, -1, (LPARAM)strdefault), 0);
					SendDlgItemMessage(hWnd,IDC_MSAA,CB_DELETESTRING,
						SendDlgItemMessage(hWnd,IDC_MSAA,CB_FINDSTRING,-1,(LPARAM)strdefault),0);
					SendDlgItemMessage(hWnd,IDC_ANISO,CB_DELETESTRING,
						SendDlgItemMessage(hWnd,IDC_ANISO,CB_FINDSTRING,-1,(LPARAM)strdefault),0);
					SendDlgItemMessage(hWnd,IDC_TEXFILTER,CB_DELETESTRING,
						SendDlgItemMessage(hWnd,IDC_TEXFILTER,CB_FINDSTRING,-1,(LPARAM)strdefault),0);
					SendDlgItemMessage(hWnd,IDC_ASPECT3D,CB_DELETESTRING,
						SendDlgItemMessage(hWnd,IDC_ASPECT3D,CB_FINDSTRING,-1,(LPARAM)strdefault),0);
					SendDlgItemMessage(hWnd,IDC_HIGHRES,BM_SETSTYLE,BS_AUTOCHECKBOX,(LPARAM)TRUE);
					SendDlgItemMessage(hWnd,IDC_TEXTUREFORMAT,CB_DELETESTRING,
						SendDlgItemMessage(hWnd,IDC_ASPECT3D,CB_FINDSTRING,-1,(LPARAM)strdefault),0);
					SendDlgItemMessage(hWnd,IDC_TEXUPLOAD,CB_DELETESTRING,
						SendDlgItemMessage(hWnd,IDC_ASPECT3D,CB_FINDSTRING,-1,(LPARAM)strdefault),0);
				*/
				}
				// Read settings into controls
				// Display tab
				SetCombo(hTabs[0], IDC_VIDMODE, cfg->scaler, cfgmask->scaler, tristate);
				SetCombo(hTabs[0], IDC_COLORDEPTH, 0, 0, tristate);
				SetCombo(hTabs[0], IDC_SCALE, cfg->scalingfilter, cfgmask->scalingfilter, tristate);
				SetCombo(hTabs[0], IDC_EXTRAMODES, 0, 0, tristate);
				SetAspectCombo(hTabs[0], IDC_ASPECT, cfg->aspect, cfgmask->aspect, tristate);
				SetCombo(hTabs[0], IDC_SORTMODES, cfg->SortModes, cfgmask->SortModes, tristate);
				SetCombo(hTabs[0], IDC_DPISCALE, cfg->DPIScale, cfgmask->DPIScale, tristate);
				SetCombo(hTabs[0], IDC_VSYNC, cfg->vsync, cfgmask->vsync, tristate);
				SetCombo(hTabs[0], IDC_FULLMODE, cfg->fullmode, cfgmask->fullmode, tristate);
				SetCheck(hTabs[0], IDC_COLOR, cfg->colormode, cfgmask->colormode, tristate);
				/*
				SetCombo(hWnd,IDC_POSTSCALE,cfg->firstscalefilter,cfgmask->firstscalefilter,tristate);
				SetCombo(hWnd,IDC_MSAA,cfg->msaa,cfgmask->msaa,tristate);
				SetCombo(hWnd,IDC_ANISO,cfg->anisotropic,cfgmask->anisotropic,tristate);
				SetCombo(hWnd,IDC_TEXFILTER,cfg->texfilter,cfgmask->texfilter,tristate);
				SetCombo(hWnd,IDC_ASPECT3D,cfg->aspect3d,cfgmask->aspect3d,tristate);
				SetCheck(hWnd,IDC_HIGHRES,cfg->highres,cfgmask->highres,tristate);
				SetCheck(hWnd,IDC_UNCOMMONCOLOR,cfg->AllColorDepths,cfgmask->AllColorDepths,tristate);
				SetCombo(hWnd,IDC_TEXTUREFORMAT,cfg->TextureFormat,cfgmask->TextureFormat,tristate);
				SetCombo(hWnd,IDC_TEXUPLOAD,cfg->TexUpload,cfgmask->TexUpload,tristate);
				SetCheck(hWnd,IDC_EXTRAMODES,cfg->ExtraModes,cfgmask->ExtraModes,tristate);
				SetText(hWnd,IDC_SHADER,cfg->shaderfile,cfgmask->shaderfile,tristate);
				SetPostScaleCombo(hWnd, IDC_POSTSCALESIZE, cfg->firstscalex, cfg->firstscaley,
					cfgmask->firstscalex, cfgmask->firstscaley, tristate);
				*/
			}
			break;/*
		case IDC_POSTSCALE:
			cfg->firstscalefilter = GetCombo(hWnd,IDC_POSTSCALE,&cfgmask->firstscalefilter);
			EnableWindow(GetDlgItem(hWnd,IDC_APPLY),TRUE);
			*dirty = TRUE;
			break;
		case IDC_MSAA:
			cfg->msaa = GetCombo(hWnd,IDC_MSAA,&cfgmask->msaa);
			EnableWindow(GetDlgItem(hWnd,IDC_APPLY),TRUE);
			*dirty = TRUE;
			break;
		case IDC_ANISO:
			cfg->anisotropic = GetCombo(hWnd,IDC_ANISO,&cfgmask->anisotropic);
			EnableWindow(GetDlgItem(hWnd,IDC_APPLY),TRUE);
			*dirty = TRUE;
			break;
		case IDC_TEXFILTER:
			cfg->texfilter = GetCombo(hWnd,IDC_TEXFILTER,&cfgmask->texfilter);
			EnableWindow(GetDlgItem(hWnd,IDC_APPLY),TRUE);
			*dirty = TRUE;
			break;
		case IDC_ASPECT3D:
			cfg->aspect3d = GetCombo(hWnd,IDC_ASPECT3D,&cfgmask->aspect3d);
			EnableWindow(GetDlgItem(hWnd,IDC_APPLY),TRUE);
			*dirty = TRUE;
			break;
		case IDC_HIGHRES:
			cfg->highres = GetCheck(hWnd,IDC_HIGHRES,&cfgmask->highres);
			EnableWindow(GetDlgItem(hWnd,IDC_APPLY),TRUE);
			*dirty = TRUE;
			break;
		case IDC_UNCOMMONCOLOR:
			cfg->AllColorDepths = GetCheck(hWnd,IDC_UNCOMMONCOLOR,&cfgmask->AllColorDepths);
			EnableWindow(GetDlgItem(hWnd,IDC_APPLY),TRUE);
			*dirty = TRUE;
			break;
		case IDC_TEXTUREFORMAT:
			cfg->TextureFormat = GetCombo(hWnd,IDC_TEXTUREFORMAT,&cfgmask->TextureFormat);
			EnableWindow(GetDlgItem(hWnd,IDC_APPLY),TRUE);
			*dirty = TRUE;
			break;
		case IDC_TEXUPLOAD:
			cfg->TexUpload = GetCombo(hWnd,IDC_TEXUPLOAD,&cfgmask->TexUpload);
			EnableWindow(GetDlgItem(hWnd,IDC_APPLY),TRUE);
			*dirty = TRUE;
			break;
		case IDC_POSTSCALESIZE:
			if (HIWORD(wParam) == CBN_KILLFOCUS)
			{
				GetPostScaleCombo(hWnd, IDC_POSTSCALESIZE, &cfg->firstscalex, &cfg->firstscaley,
					&cfgmask->firstscalex, &cfgmask->firstscaley);
				SetPostScaleCombo(hWnd, IDC_POSTSCALESIZE, cfg->firstscalex, cfg->firstscaley,
					cfgmask->firstscalex, cfgmask->firstscaley, tristate);
				EnableWindow(GetDlgItem(hWnd, IDC_APPLY), TRUE);
				*dirty = TRUE;
			}
			else if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				GetPostScaleCombo(hWnd, IDC_POSTSCALESIZE, &cfg->firstscalex, &cfg->firstscaley,
					&cfgmask->firstscalex, &cfgmask->firstscaley);
				EnableWindow(GetDlgItem(hWnd, IDC_APPLY), TRUE);
				*dirty = TRUE;
			}
			break;
		case IDC_SHADER:
			if(HIWORD(wParam) == EN_CHANGE)
			{
				GetText(hWnd,IDC_SHADER,cfg->shaderfile,cfgmask->shaderfile);
				EnableWindow(GetDlgItem(hWnd,IDC_APPLY),TRUE);
				*dirty = TRUE;
			}
			break;
*/
		case IDC_ADD:
			selectedfile[0] = 0;
			ZeroMemory(&filename, OPENFILENAME_SIZE_VERSION_400);
			filename.lStructSize = OPENFILENAME_SIZE_VERSION_400;
			filename.hwndOwner = hWnd;
			filename.lpstrFilter = exe_filter;
			filename.lpstrFile = selectedfile;
			filename.nMaxFile = MAX_PATH + 1;
			filename.lpstrInitialDir = _T("%ProgramFiles%");
			filename.lpstrTitle = _T("Select program");
			filename.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
			if (GetOpenFileName(&filename))
			{
				if (CheckProfileExists(filename.lpstrFile))
				{
					MessageBox(hWnd, _T("A profile already exists for this program."),
						_T("Profile already exists"), MB_OK | MB_ICONWARNING);
					break;
				}
				err = AddApp(filename.lpstrFile, TRUE, FALSE, FALSE, hWnd);
				if (!err)
				{
					LPTSTR newkey = MakeNewConfig(filename.lpstrFile);
					LPTSTR newkey2 = (LPTSTR)malloc((_tcslen(newkey) + 24) * sizeof(TCHAR));
					_tcscpy(newkey2, _T("Software\\DXGL\\Profiles\\"));
					_tcscat(newkey2, newkey);
					appcount++;
					if (appcount > maxapps)
					{
						maxapps += 128;
						apps = (app_setting *)realloc(apps, maxapps * sizeof(app_setting));
					}
					RegOpenKeyEx(HKEY_CURRENT_USER, newkey2, 0, KEY_READ, &hKey);
					RegQueryValueEx(hKey, _T("InstallPath"), NULL, NULL, NULL, &buffersize);
					regbuffer = (LPTSTR)malloc(buffersize);
					regbuffer[0] = 0;
					error = RegQueryValueEx(hKey, _T("InstallPath"), NULL, NULL, (LPBYTE)regbuffer, &buffersize);
					apps[appcount - 1].regkey = (LPTSTR)malloc((_tcslen(newkey) + 1) * sizeof(TCHAR));
					_tcscpy(apps[appcount - 1].regkey, newkey);
					GetConfig(&apps[appcount - 1].cfg, &apps[appcount - 1].mask, newkey);
					apps[appcount - 1].dirty = FALSE;
					free(newkey2);
					if ((regbuffer[0] == 0) || error != ERROR_SUCCESS)
					{
						// Default icon
						apps[appcount - 1].icon = LoadIcon(NULL, IDI_APPLICATION);
						apps[appcount - 1].icon_shared = TRUE;
						apps[appcount - 1].name = (LPTSTR)malloc((_tcslen(newkey) + 1) * sizeof(TCHAR));
						_tcscpy(apps[appcount - 1].name, newkey);
						break;
					}
					if (_tcsrchr(newkey, _T('-'))) *(_tcsrchr(newkey, _T('-'))) = 0;
					path = (LPTSTR)malloc(((_tcslen(regbuffer) + _tcslen(newkey) + 2)) * sizeof(TCHAR));
					_tcscpy(path, regbuffer);
					_tcscpy(apps[appcount - 1].path, path);
					_tcscat(path, _T("\\"));
					_tcscat(path, newkey);
					if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES)
					{
						// Default icon
						apps[appcount - 1].icon = LoadIcon(NULL, IDI_APPLICATION);
						apps[appcount - 1].icon_shared = TRUE;
						apps[appcount - 1].name = (LPTSTR)malloc((_tcslen(newkey) + 1) * sizeof(TCHAR));
						_tcscpy(apps[appcount - 1].name, newkey);
						break;
					}
					else
					{
						// Get exe attributes
						error = SHGetFileInfo(path, 0, &fileinfo, sizeof(SHFILEINFO), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_ADDOVERLAYS);
						apps[appcount - 1].icon = fileinfo.hIcon;
						apps[appcount - 1].icon_shared = FALSE;
						verinfosize = GetFileVersionInfoSize(path, NULL);
						verinfo = malloc(verinfosize);
						hasname = FALSE;
						if (GetFileVersionInfo(path, 0, verinfosize, verinfo))
						{
							if (VerQueryValue(verinfo, _T("\\VarFileInfo\\Translation"), (LPVOID*)&outbuffer, &outlen))
							{
								memcpy(translation, outbuffer, 4);
								_sntprintf(verpath, 64, _T("\\StringFileInfo\\%04x%04x\\FileDescription"), translation[0], translation[1]);
								if (VerQueryValue(verinfo, verpath, (LPVOID*)&outbuffer, &outlen))
								{
									hasname = TRUE;
									apps[appcount - 1].name = (LPTSTR)malloc((_tcslen(outbuffer) + 1) * sizeof(TCHAR));
									_tcscpy(apps[appcount - 1].name, outbuffer);
								}
								else
								{
									_sntprintf(verpath, 64, _T("\\StringFileInfo\\%04x%04x\\ProductName"), ((WORD*)outbuffer)[0], ((WORD*)outbuffer)[1]);
									if (VerQueryValue(verinfo, verpath, (LPVOID*)&outbuffer, &outlen))
									{
										hasname = TRUE;
										apps[appcount - 1].name = (LPTSTR)malloc((_tcslen(outbuffer) + 1) * sizeof(TCHAR));
										_tcscpy(apps[appcount - 1].name, outbuffer);
									}
									else
									{
										_sntprintf(verpath, 64, _T("\\StringFileInfo\\%04x%04x\\InternalName"), ((WORD*)outbuffer)[0], ((WORD*)outbuffer)[1]);
										if (VerQueryValue(verinfo, verpath, (LPVOID*)&outbuffer, &outlen))
										{
											hasname = TRUE;
											apps[appcount - 1].name = (LPTSTR)malloc((_tcslen(outbuffer) + 1) * sizeof(TCHAR));
											_tcscpy(apps[appcount - 1].name, outbuffer);
										}
									}
								}
							}
						}
						if (!hasname)
						{
							apps[appcount - 1].name = (LPTSTR)malloc((_tcslen(newkey) + 1) * sizeof(TCHAR));
							_tcscpy(apps[appcount - 1].name, newkey);
						}
						free(verinfo);
						free(path);
					}
					SendDlgItemMessage(hWnd, IDC_APPS, LB_SETCURSEL,
						SendDlgItemMessage(hWnd, IDC_APPS, LB_ADDSTRING, 0, (LPARAM)apps[appcount - 1].name), 0);
					SendMessage(hWnd, WM_COMMAND, IDC_APPS + 0x10000, 0);
					RegCloseKey(hKey);
					free(regbuffer);
				}
			}
			break;
		case IDC_REMOVE:
			if(MessageBox(hWnd,_T("Do you want to delete the selected application profile and remove DXGL from its installation folder(s)?"),
				_T("Confirmation"),MB_YESNO|MB_ICONQUESTION) != IDYES) return FALSE;
			regpath = (LPTSTR)malloc((_tcslen(apps[current_app].regkey) + 15)*sizeof(TCHAR));
			_tcscpy(regpath, _T("Software\\DXGL\\Profiles\\"));
			_tcscat(regpath, apps[current_app].regkey);
			regkey = (LPTSTR)malloc(_tcslen(apps[current_app].regkey));
			_tcscpy(regkey, apps[current_app].regkey);
			RegOpenKeyEx(HKEY_CURRENT_USER,regpath,0,KEY_READ,&hKey);
			RegQueryValueEx(hKey,_T("InstallPath"),NULL,NULL,NULL,&buffersize);
			regbuffer = (LPTSTR)malloc(buffersize);
			regbuffer[0] = 0;
			failed = FALSE;
			error = RegQueryValueEx(hKey, _T("InstallPath"), NULL, NULL, (LPBYTE)regbuffer, &buffersize);
			path = (LPTSTR)malloc(((_tcslen(regbuffer) + 12)) * sizeof(TCHAR));
			_tcscpy(path, regbuffer);
			_tcscat(path, _T("\\ddraw.dll"));
			if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
			{
				if (DelApp(path, FALSE, hWnd)) failed = TRUE;
			}
			free(path);
			free(regbuffer);
			RegCloseKey(hKey);
			if(!failed)
			{
				RegDeleteKey(HKEY_CURRENT_USER,regpath);
				if(!apps[current_app].icon_shared) DeleteObject(apps[current_app].icon);
				if(apps[current_app].name) free(apps[current_app].name);
				if(apps[current_app].regkey) free(apps[current_app].regkey);
				for(i = current_app; i < appcount; i++)
				{
					apps[i] = apps[i+1];
				}
				appcount--;
			}
			SendDlgItemMessage(hWnd,IDC_APPS,LB_DELETESTRING,current_app,0);
			SendDlgItemMessage(hWnd, IDC_APPS, LB_SETCURSEL, 0, 0);
			SendMessage(hWnd, WM_COMMAND, IDC_APPS + 0x10000, 0);
			break;
		}
		break;
	}
	return FALSE;
}

void UpgradeDXGL()
{
	HKEY hKeyBase;
	HKEY hKey;
	DWORD keysize, keysize2;
	int i = 0;
	LONG error;
	LPTSTR keyname;
	DWORD sizeout;
	DWORD buffersize;
	DWORD regbuffersize;
	LPTSTR regbuffer;
	BOOL installed = FALSE;
	BOOL dxgl_installdir = FALSE;
	BOOL old_dxgl = FALSE;
	HKEY hKeyInstall;
	TCHAR installpath[MAX_PATH + 1];
	TCHAR srcpath[MAX_PATH + 1];
	TCHAR destpath[MAX_PATH + 1];
	TCHAR inipath[MAX_PATH + 1];
	TCHAR backuppath[MAX_PATH + 1];
	app_ini_options inioptions;
	HMODULE hmod;
	UpgradeConfig();
	regbuffersize = 1024;
	regbuffer = (LPTSTR)malloc(regbuffersize * sizeof(TCHAR));
	RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\DXGL\\Profiles"), 0, NULL, 0, KEY_READ, NULL, &hKeyBase, NULL);
	RegQueryInfoKey(hKeyBase, NULL, NULL, NULL, NULL, &keysize, NULL, NULL, NULL, NULL, NULL, NULL);
	keysize++;
	keyname = (LPTSTR)malloc(keysize * sizeof(TCHAR));
	keysize2 = keysize;
	error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\DXGL"), 0, KEY_READ, &hKeyInstall);
	if (error == ERROR_SUCCESS)
	{
		dxgl_installdir = TRUE;
		sizeout = (MAX_PATH + 1) * sizeof(TCHAR);
		error = RegQueryValueEx(hKeyInstall, _T("InstallDir"), NULL, NULL, (LPBYTE)installpath, &sizeout);
		if (error == ERROR_SUCCESS) installed = TRUE;
	}
	if (hKeyInstall) RegCloseKey(hKeyInstall);
	if (!installed)
	{
		GetModuleFileName(NULL, installpath, MAX_PATH + 1);
	}
	if (dxgl_installdir) _tcscat(installpath, _T("\\"));
	else (_tcsrchr(installpath, _T('\\')))[1] = 0;
	_tcsncpy(srcpath, installpath, MAX_PATH + 1);
	_tcscat(srcpath, _T("ddraw.dll"));
	while (RegEnumKeyEx(hKeyBase, i, keyname, &keysize2, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
	{
		keysize2 = keysize;
		i++;
		error = RegOpenKeyEx(hKeyBase, keyname, 0, KEY_READ, &hKey);
		buffersize = regbuffersize;
		RegQueryValueEx(hKey, _T("InstallPath"), NULL, NULL, NULL, &buffersize);
		if (buffersize > regbuffersize)
		{
			regbuffersize = buffersize;
			regbuffer = (LPTSTR)realloc(regbuffer, regbuffersize);
		}
		buffersize = regbuffersize;
		regbuffer[0] = 0;
		error = RegQueryValueEx(hKey, _T("InstallPath"), NULL, NULL, (LPBYTE)regbuffer, &buffersize);
		if (regbuffer[0] != 0)
		{
			_tcsncpy(destpath, regbuffer, MAX_PATH + 1);
			_tcscat(destpath, _T("\\ddraw.dll"));
			_tcsncpy(inipath, regbuffer, MAX_PATH + 1);
			_tcscat(inipath, _T("\\"));
			_tcsncpy(backuppath, regbuffer, MAX_PATH + 1);
			_tcscat(backuppath, _T("\\ddraw.dll.dxgl-backup"));
			ReadAppINIOptions(inipath, &inioptions);
			error = CopyFile(srcpath, destpath, TRUE);
			if (!error)
			{
				error = GetLastError();
				if (error == ERROR_FILE_EXISTS)
				{
					if (inioptions.NoOverwrite) continue;
					if ((inioptions.sha256[0] != 0) && !memcmp(inioptions.sha256, inioptions.sha256comp, 64))
						// Detected original ddraw matches INI hash
						CopyFile(srcpath, backuppath, FALSE);
					old_dxgl = FALSE;
					error = SetErrorMode(SEM_FAILCRITICALERRORS);
					SetErrorMode(error | SEM_FAILCRITICALERRORS);
					hmod = LoadLibrary(destpath);
					SetErrorMode(error);
					if (hmod)
					{
						if (GetProcAddress(hmod, "IsDXGLDDraw")) old_dxgl = TRUE;
						FreeLibrary(hmod);
					}
					if (old_dxgl) CopyFile(srcpath, destpath, FALSE);
				}
			}
		}
		RegCloseKey(hKey);
	}
	free(regbuffer);
	free(keyname);
	RegCloseKey(hKeyBase);
}

// '0' for keep, '1' for remove, personal settings
void UninstallDXGL(TCHAR uninstall)
{
	HKEY hKeyBase;
	HKEY hKey;
	DWORD keysize, keysize2;
	LONG error;
	LPTSTR keyname;
	DWORD sizeout;
	DWORD buffersize;
	DWORD regbuffersize;
	LPTSTR regbuffer;
	BOOL installed = FALSE;
	BOOL dxgl_installdir = FALSE;
	BOOL old_dxgl = FALSE;
	HKEY hKeyInstall;
	TCHAR installpath[MAX_PATH + 1];
	TCHAR srcpath[MAX_PATH + 1];
	TCHAR destpath[MAX_PATH + 1];
	TCHAR inipath[MAX_PATH + 1];
	TCHAR backuppath[MAX_PATH + 1];
	HANDLE exists;
	app_ini_options inioptions;
	HMODULE hmod;
	int i = 0;
	UpgradeConfig();  // Just to make sure the registry format is correct
	regbuffersize = 1024;
	regbuffer = (LPTSTR)malloc(regbuffersize * sizeof(TCHAR));
	error = RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\DXGL\\Profiles"), 0, KEY_ALL_ACCESS, &hKeyBase);
	if (error != ERROR_SUCCESS) return;
	RegQueryInfoKey(hKeyBase, NULL, NULL, NULL, NULL, &keysize, NULL, NULL, NULL, NULL, NULL, NULL);
	keysize++;
	keyname = (LPTSTR)malloc(keysize * sizeof(TCHAR));
	keysize2 = keysize;
	error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\DXGL"), 0, KEY_READ, &hKeyInstall);
	if (error == ERROR_SUCCESS)
	{
		dxgl_installdir = TRUE;
		sizeout = (MAX_PATH + 1) * sizeof(TCHAR);
		error = RegQueryValueEx(hKeyInstall, _T("InstallDir"), NULL, NULL, (LPBYTE)installpath, &sizeout);
		if (error == ERROR_SUCCESS) installed = TRUE;
	}
	if (hKeyInstall) RegCloseKey(hKeyInstall);
	if (!installed)
	{
		GetModuleFileName(NULL, installpath, MAX_PATH + 1);
	}
	if (dxgl_installdir) _tcscat(installpath, _T("\\"));
	else (_tcsrchr(installpath, _T('\\')))[1] = 0;
	_tcsncpy(srcpath, installpath, MAX_PATH + 1);
	_tcscat(srcpath, _T("ddraw.dll"));
	while (RegEnumKeyEx(hKeyBase, i, keyname, &keysize2, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
	{
		keysize2 = keysize;
		i++;
		error = RegOpenKeyEx(hKeyBase, keyname, 0, KEY_READ, &hKey);
		buffersize = regbuffersize;
		if (buffersize > regbuffersize)
		{
			regbuffersize = buffersize;
			regbuffer = (LPTSTR)realloc(regbuffer, regbuffersize);
		}
		buffersize = regbuffersize;
		regbuffer[0] = 0;
		error = RegQueryValueEx(hKey, _T("InstallPath"), NULL, NULL, (LPBYTE)regbuffer, &buffersize);
		if (regbuffer[0] != 0)
		{
			_tcsncpy(destpath, regbuffer, MAX_PATH + 1);
			_tcscat(destpath, _T("\\ddraw.dll"));
			_tcsncpy(inipath, regbuffer, MAX_PATH + 1);
			_tcscat(inipath, _T("\\"));
			_tcsncpy(backuppath, regbuffer, MAX_PATH + 1);
			_tcscat(backuppath, _T("\\ddraw.dll.dxgl-backup"));
			ReadAppINIOptions(inipath, &inioptions);
			if (inioptions.NoOverwrite || inioptions.NoUninstall) continue;
			if (GetFileAttributes(destpath) != INVALID_FILE_ATTRIBUTES)
			{
				old_dxgl = FALSE;
				error = SetErrorMode(SEM_FAILCRITICALERRORS);
				SetErrorMode(error | SEM_FAILCRITICALERRORS);
				hmod = LoadLibrary(destpath);
				SetErrorMode(error);
				if (hmod)
				{
					if (GetProcAddress(hmod, "IsDXGLDDraw")) old_dxgl = TRUE;
					FreeLibrary(hmod);
				}
				if (_tcscmp(srcpath, destpath))
				{
					if (old_dxgl)
					{
						DeleteFile(destpath);
						exists = CreateFile(backuppath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
							NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
						if (exists == INVALID_HANDLE_VALUE) continue;
						else
						{
							CloseHandle(exists);
							MoveFile(backuppath, destpath);
						}
					}
				}
			}
		}
		RegCloseKey(hKey);
	}
	free(regbuffer);
	RegQueryInfoKey(hKeyBase, NULL, NULL, NULL, NULL, &keysize, NULL, NULL, NULL, NULL, NULL, NULL);
	keysize++;
	if (uninstall == '1')  // Delete user settings
	{
		while (RegEnumKeyEx(hKeyBase, 0, keyname, &keysize2, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			keysize2 = keysize;
			i++;
			RegDeleteKey(hKeyBase, keyname);
		}
		RegCloseKey(hKeyBase);
		RegDeleteKey(HKEY_CURRENT_USER, _T("Software\\DXGL\\Profiles"));
		RegDeleteKey(HKEY_CURRENT_USER, _T("Software\\DXGL\\Global"));
		RegDeleteKey(HKEY_CURRENT_USER, _T("Software\\DXGL"));
	}
	else RegCloseKey(hKeyBase);
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR    lpCmdLine, int nCmdShow)
{
	INITCOMMONCONTROLSEX icc;
	HMODULE comctl32;
	BOOL(WINAPI *iccex)(LPINITCOMMONCONTROLSEX lpInitCtrls);
	HANDLE hMutex;
	HWND hWnd;
	osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osver);
	CoInitialize(NULL);
	if (!_tcsnicmp(lpCmdLine, _T("upgrade"), 7))
	{
		UpgradeDXGL();
		return 0;
	}
	if (!_tcsnicmp(lpCmdLine, _T("uninstall"), 9))
	{
		UninstallDXGL(lpCmdLine[10]);
		return 0;
	}
	if(!_tcsnicmp(lpCmdLine,_T("install "),8))
	{
		return AddApp(lpCmdLine+8,TRUE,TRUE,FALSE,NULL);
	}
	if(!_tcsnicmp(lpCmdLine,_T("forceinstall "),13))
	{
		return AddApp(lpCmdLine+8,TRUE,TRUE,TRUE,NULL);
	}
	if(!_tcsnicmp(lpCmdLine,_T("remove "),7))
	{
		return DelApp(lpCmdLine+7,TRUE,NULL);
	}
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_WIN95_CLASSES;
	comctl32 = LoadLibrary(_T("comctl32.dll"));
	iccex = (BOOL (WINAPI *)(LPINITCOMMONCONTROLSEX))GetProcAddress(comctl32,"InitCommonControlsEx");
	if(iccex) iccex(&icc);
	else InitCommonControls();
	hinstance = hInstance;
	GetModuleFileName(NULL,hlppath,MAX_PATH);
	GetDirFromPath(hlppath);
	_tcscat(hlppath,_T("\\dxgl.chm"));
	hMutex = CreateMutex(NULL, TRUE, _T("DXGLConfigMutex"));
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		// Find DXGL Config window
		hWnd = FindWindow(NULL, _T("DXGL Config (Experimental)"));
		// Focus DXGL Config window
		if (hWnd) SetForegroundWindow(hWnd);
		return 0;
	}
	DialogBox(hInstance,MAKEINTRESOURCE(IDD_DXGLCFG),0,(DLGPROC)DXGLCfgCallback);
	ReleaseMutex(hMutex);
	CloseHandle(hMutex);
#ifdef _DEBUG
	_CrtDumpMemoryLeaks();
#endif
	return 0;
}
