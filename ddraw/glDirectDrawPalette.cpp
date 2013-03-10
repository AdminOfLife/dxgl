// DXGL
// Copyright (C) 2011-2012 William Feely

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

#include "common.h"
#include "glDirectDraw.h"
#include "glDirectDrawPalette.h"


unsigned char DefaultPalette[1024] = {
0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x80,0x80,0x00,0x00,
0x00,0x00,0x80,0x00,0x80,0x00,0x80,0x00,0x00,0x80,0x80,0x00,0xc0,0xc0,0xc0,0x00,
0xa0,0xa0,0xa0,0x00,0xf0,0xf0,0xf0,0x00,0x40,0x20,0x00,0x00,0x60,0x20,0x00,0x00,
0x80,0x20,0x00,0x00,0xa0,0x20,0x00,0x00,0xc0,0x20,0x00,0x00,0xe0,0x20,0x00,0x00,
0x00,0x40,0x00,0x00,0x20,0x40,0x00,0x00,0x40,0x40,0x00,0x00,0x60,0x40,0x00,0x00,
0x80,0x40,0x00,0x00,0xa0,0x40,0x00,0x00,0xc0,0x40,0x00,0x00,0xe0,0x40,0x00,0x00,
0x00,0x60,0x00,0x00,0x20,0x60,0x00,0x00,0x40,0x60,0x00,0x00,0x60,0x60,0x00,0x00,
0x80,0x60,0x00,0x00,0xa0,0x60,0x00,0x00,0xc0,0x60,0x00,0x00,0xe0,0x60,0x00,0x00,
0x00,0x80,0x00,0x00,0x20,0x80,0x00,0x00,0x40,0x80,0x00,0x00,0x60,0x80,0x00,0x00,
0x80,0x80,0x00,0x00,0xa0,0x80,0x00,0x00,0xc0,0x80,0x00,0x00,0xe0,0x80,0x00,0x00,
0x00,0xa0,0x00,0x00,0x20,0xa0,0x00,0x00,0x40,0xa0,0x00,0x00,0x60,0xa0,0x00,0x00,
0x80,0xa0,0x00,0x00,0xa0,0xa0,0x00,0x00,0xc0,0xa0,0x00,0x00,0xe0,0xa0,0x00,0x00,
0x00,0xc0,0x00,0x00,0x20,0xc0,0x00,0x00,0x40,0xc0,0x00,0x00,0x60,0xc0,0x00,0x00,
0x80,0xc0,0x00,0x00,0xa0,0xc0,0x00,0x00,0xc0,0xc0,0x00,0x00,0xe0,0xc0,0x00,0x00,
0x00,0xe0,0x00,0x00,0x20,0xe0,0x00,0x00,0x40,0xe0,0x00,0x00,0x60,0xe0,0x00,0x00,
0x80,0xe0,0x00,0x00,0xa0,0xe0,0x00,0x00,0xc0,0xe0,0x00,0x00,0xe0,0xe0,0x00,0x00,
0x00,0x00,0x40,0x00,0x20,0x00,0x40,0x00,0x40,0x00,0x40,0x00,0x60,0x00,0x40,0x00,
0x80,0x00,0x40,0x00,0xa0,0x00,0x40,0x00,0xc0,0x00,0x40,0x00,0xe0,0x00,0x40,0x00,
0x00,0x20,0x40,0x00,0x20,0x20,0x40,0x00,0x40,0x20,0x40,0x00,0x60,0x20,0x40,0x00,
0x80,0x20,0x40,0x00,0xa0,0x20,0x40,0x00,0xc0,0x20,0x40,0x00,0xe0,0x20,0x40,0x00,
0x00,0x40,0x40,0x00,0x20,0x40,0x40,0x00,0x40,0x40,0x40,0x00,0x60,0x40,0x40,0x00,
0x80,0x40,0x40,0x00,0xa0,0x40,0x40,0x00,0xc0,0x40,0x40,0x00,0xe0,0x40,0x40,0x00,
0x00,0x60,0x40,0x00,0x20,0x60,0x40,0x00,0x40,0x60,0x40,0x00,0x60,0x60,0x40,0x00,
0x80,0x60,0x40,0x00,0xa0,0x60,0x40,0x00,0xc0,0x60,0x40,0x00,0xe0,0x60,0x40,0x00,
0x00,0x80,0x40,0x00,0x20,0x80,0x40,0x00,0x40,0x80,0x40,0x00,0x60,0x80,0x40,0x00,
0x80,0x80,0x40,0x00,0xa0,0x80,0x40,0x00,0xc0,0x80,0x40,0x00,0xe0,0x80,0x40,0x00,
0x00,0xa0,0x40,0x00,0x20,0xa0,0x40,0x00,0x40,0xa0,0x40,0x00,0x60,0xa0,0x40,0x00,
0x80,0xa0,0x40,0x00,0xa0,0xa0,0x40,0x00,0xc0,0xa0,0x40,0x00,0xe0,0xa0,0x40,0x00,
0x00,0xc0,0x40,0x00,0x20,0xc0,0x40,0x00,0x40,0xc0,0x40,0x00,0x60,0xc0,0x40,0x00,
0x80,0xc0,0x40,0x00,0xa0,0xc0,0x40,0x00,0xc0,0xc0,0x40,0x00,0xe0,0xc0,0x40,0x00,
0x00,0xe0,0x40,0x00,0x20,0xe0,0x40,0x00,0x40,0xe0,0x40,0x00,0x60,0xe0,0x40,0x00,
0x80,0xe0,0x40,0x00,0xa0,0xe0,0x40,0x00,0xc0,0xe0,0x40,0x00,0xe0,0xe0,0x40,0x00,
0x00,0x00,0x80,0x00,0x20,0x00,0x80,0x00,0x40,0x00,0x80,0x00,0x60,0x00,0x80,0x00,
0x80,0x00,0x80,0x00,0xa0,0x00,0x80,0x00,0xc0,0x00,0x80,0x00,0xe0,0x00,0x80,0x00,
0x00,0x20,0x80,0x00,0x20,0x20,0x80,0x00,0x40,0x20,0x80,0x00,0x60,0x20,0x80,0x00,
0x80,0x20,0x80,0x00,0xa0,0x20,0x80,0x00,0xc0,0x20,0x80,0x00,0xe0,0x20,0x80,0x00,
0x00,0x40,0x80,0x00,0x20,0x40,0x80,0x00,0x40,0x40,0x80,0x00,0x60,0x40,0x80,0x00,
0x80,0x40,0x80,0x00,0xa0,0x40,0x80,0x00,0xc0,0x40,0x80,0x00,0xe0,0x40,0x80,0x00,
0x00,0x60,0x80,0x00,0x20,0x60,0x80,0x00,0x40,0x60,0x80,0x00,0x60,0x60,0x80,0x00,
0x80,0x60,0x80,0x00,0xa0,0x60,0x80,0x00,0xc0,0x60,0x80,0x00,0xe0,0x60,0x80,0x00,
0x00,0x80,0x80,0x00,0x20,0x80,0x80,0x00,0x40,0x80,0x80,0x00,0x60,0x80,0x80,0x00,
0x80,0x80,0x80,0x00,0xa0,0x80,0x80,0x00,0xc0,0x80,0x80,0x00,0xe0,0x80,0x80,0x00,
0x00,0xa0,0x80,0x00,0x20,0xa0,0x80,0x00,0x40,0xa0,0x80,0x00,0x60,0xa0,0x80,0x00,
0x80,0xa0,0x80,0x00,0xa0,0xa0,0x80,0x00,0xc0,0xa0,0x80,0x00,0xe0,0xa0,0x80,0x00,
0x00,0xc0,0x80,0x00,0x20,0xc0,0x80,0x00,0x40,0xc0,0x80,0x00,0x60,0xc0,0x80,0x00,
0x80,0xc0,0x80,0x00,0xa0,0xc0,0x80,0x00,0xc0,0xc0,0x80,0x00,0xe0,0xc0,0x80,0x00,
0x00,0xe0,0x80,0x00,0x20,0xe0,0x80,0x00,0x40,0xe0,0x80,0x00,0x60,0xe0,0x80,0x00,
0x80,0xe0,0x80,0x00,0xa0,0xe0,0x80,0x00,0xc0,0xe0,0x80,0x00,0xe0,0xe0,0x80,0x00,
0x00,0x00,0xc0,0x00,0x20,0x00,0xc0,0x00,0x40,0x00,0xc0,0x00,0x60,0x00,0xc0,0x00,
0x80,0x00,0xc0,0x00,0xa0,0x00,0xc0,0x00,0xc0,0x00,0xc0,0x00,0xe0,0x00,0xc0,0x00,
0x00,0x20,0xc0,0x00,0x20,0x20,0xc0,0x00,0x40,0x20,0xc0,0x00,0x60,0x20,0xc0,0x00,
0x80,0x20,0xc0,0x00,0xa0,0x20,0xc0,0x00,0xc0,0x20,0xc0,0x00,0xe0,0x20,0xc0,0x00,
0x00,0x40,0xc0,0x00,0x20,0x40,0xc0,0x00,0x40,0x40,0xc0,0x00,0x60,0x40,0xc0,0x00,
0x80,0x40,0xc0,0x00,0xa0,0x40,0xc0,0x00,0xc0,0x40,0xc0,0x00,0xe0,0x40,0xc0,0x00,
0x00,0x60,0xc0,0x00,0x20,0x60,0xc0,0x00,0x40,0x60,0xc0,0x00,0x60,0x60,0xc0,0x00,
0x80,0x60,0xc0,0x00,0xa0,0x60,0xc0,0x00,0xc0,0x60,0xc0,0x00,0xe0,0x60,0xc0,0x00,
0x00,0x80,0xc0,0x00,0x20,0x80,0xc0,0x00,0x40,0x80,0xc0,0x00,0x60,0x80,0xc0,0x00,
0x80,0x80,0xc0,0x00,0xa0,0x80,0xc0,0x00,0xc0,0x80,0xc0,0x00,0xe0,0x80,0xc0,0x00,
0x00,0xa0,0xc0,0x00,0x20,0xa0,0xc0,0x00,0x40,0xa0,0xc0,0x00,0x60,0xa0,0xc0,0x00,
0x80,0xa0,0xc0,0x00,0xa0,0xa0,0xc0,0x00,0xc0,0xa0,0xc0,0x00,0xe0,0xa0,0xc0,0x00,
0x00,0xc0,0xc0,0x00,0x20,0xc0,0xc0,0x00,0x40,0xc0,0xc0,0x00,0x60,0xc0,0xc0,0x00,
0x80,0xc0,0xc0,0x00,0xa0,0xc0,0xc0,0x00,0xff,0xfb,0xf0,0x00,0xa0,0xa0,0xa4,0x00,
0x80,0x80,0x80,0x00,0xff,0x00,0x00,0x00,0x00,0xff,0x00,0x00,0xff,0xff,0x00,0x00,
0x00,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0x00,0xff,0xff,0x00,0xff,0xff,0xff,0x00
}; // Grabbed from a Windows DDRAW run


glDirectDrawPalette::glDirectDrawPalette(DWORD dwFlags, LPPALETTEENTRY lpDDColorArray, LPDIRECTDRAWPALETTE FAR *lplpDDPalette)
{
	TRACE_ENTER(4,14,this,9,dwFlags,14,lpDDColorArray,14,lplpDDPalette);
	refcount = 1;
	flags = dwFlags;
	if(lpDDColorArray == NULL) memcpy(palette,DefaultPalette,1024);
	else
	{
		if(flags & DDPCAPS_1BIT)
			if(flags & DDPCAPS_8BITENTRIES) memcpy(palette,lpDDColorArray,2);
			else memcpy(palette,lpDDColorArray,2*sizeof(PALETTEENTRY));
		else if(flags & DDPCAPS_2BIT)
			if(flags & DDPCAPS_8BITENTRIES) memcpy(palette,lpDDColorArray,4);
			else memcpy(palette,lpDDColorArray,4*sizeof(PALETTEENTRY));
		else if(flags & DDPCAPS_4BIT)
			if(flags & DDPCAPS_8BITENTRIES) memcpy(palette,lpDDColorArray,16);
			else memcpy(palette,lpDDColorArray,16*sizeof(PALETTEENTRY));
		else memcpy(palette,lpDDColorArray,256*sizeof(PALETTEENTRY));
	}
	if(lplpDDPalette) *lplpDDPalette = this;
	TRACE_EXIT(-1,0);
}

glDirectDrawPalette::~glDirectDrawPalette()
{
	TRACE_ENTER(1,14,this);
	TRACE_EXIT(-1,0);
}

HRESULT WINAPI glDirectDrawPalette::QueryInterface(REFIID riid, void** ppvObj)
{
	TRACE_ENTER(3,14,this,24,&riid,14,ppvObj);
	if(!this) TRACE_RET(23,DDERR_INVALIDOBJECT);
	if(!ppvObj) TRACE_RET(23,DDERR_INVALIDPARAMS);
	if(riid == IID_IUnknown)
	{
		this->AddRef();
		*ppvObj = this;
		TRACE_VAR("*ppvObj",14,*ppvObj);
		TRACE_EXIT(23,DD_OK);
		return DD_OK;
	}
	TRACE_EXIT(23,E_NOINTERFACE);
	ERR(E_NOINTERFACE);
}

ULONG WINAPI glDirectDrawPalette::AddRef()
{
	TRACE_ENTER(1,14,this);
	if(!this) return 0;
	refcount++;
	TRACE_EXIT(8,refcount);
	return refcount;
}

ULONG WINAPI glDirectDrawPalette::Release()
{
	TRACE_ENTER(1,14,this);
	if(!this) return 0;
	ULONG ret;
	refcount--;
	ret = refcount;
	if(refcount == 0) delete this;
	TRACE_EXIT(8,ret);
	return ret;
}

HRESULT WINAPI glDirectDrawPalette::GetCaps(LPDWORD lpdwCaps)
{
	TRACE_ENTER(2,14,this,14,lpdwCaps);
	if(!this) TRACE_RET(23,DDERR_INVALIDOBJECT);
	*lpdwCaps = flags;
	TRACE_VAR("*lpdwCaps",9,*lpdwCaps);
	TRACE_EXIT(23,DD_OK);
	return DD_OK;
}

HRESULT WINAPI glDirectDrawPalette::GetEntries(DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries)
{
	TRACE_ENTER(5,14,this,9,dwFlags,8,dwBase,8,dwNumEntries,14,lpEntries);
	if(!this) TRACE_RET(23,DDERR_INVALIDOBJECT);
	DWORD allentries = 256;
	DWORD entrysize;
	if(flags & DDPCAPS_1BIT) allentries=2;
	if(flags & DDPCAPS_2BIT) allentries=4;
	if(flags & DDPCAPS_4BIT) allentries=16;
	if(flags & DDPCAPS_8BIT) allentries=256;
	if(flags & DDPCAPS_8BITENTRIES) entrysize = 1;
	else entrysize = sizeof(PALETTEENTRY);
	if((dwBase + dwNumEntries) > allentries) TRACE_RET(23,DDERR_INVALIDPARAMS);
	memcpy(lpEntries,((char *)palette)+(dwBase*entrysize),dwNumEntries*entrysize);
	TRACE_EXIT(23,DD_OK);
	return DD_OK;
}
HRESULT WINAPI glDirectDrawPalette::Initialize(LPDIRECTDRAW lpDD, DWORD dwFlags, LPPALETTEENTRY lpDDColorTable)
{
	TRACE_ENTER(4,14,this,14,lpDD,9,dwFlags,14,lpDDColorTable);
	if(!this) TRACE_RET(23,DDERR_INVALIDOBJECT);
	TRACE_EXIT(23,DDERR_ALREADYINITIALIZED);
	return DDERR_ALREADYINITIALIZED;
}
HRESULT WINAPI glDirectDrawPalette::SetEntries(DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries)
{
	TRACE_ENTER(5,14,this,9,dwFlags,8,dwStartingEntry,8,dwCount,14,lpEntries);
	if(!this) TRACE_RET(23,DDERR_INVALIDOBJECT);
	DWORD allentries = 256;
	DWORD entrysize;
	if(flags & DDPCAPS_1BIT) allentries=2;
	if(flags & DDPCAPS_2BIT) allentries=4;
	if(flags & DDPCAPS_4BIT) allentries=16;
	if(flags & DDPCAPS_8BIT) allentries=256;
	if(flags & DDPCAPS_8BITENTRIES) entrysize = 1;
	else entrysize = sizeof(PALETTEENTRY);
	if((dwStartingEntry + dwCount) > allentries) TRACE_RET(23,DDERR_INVALIDPARAMS);
	memcpy(((char *)palette)+(dwStartingEntry*entrysize),lpEntries,dwCount*entrysize);
	TRACE_EXIT(23,DD_OK);
	return DD_OK;
}

LPPALETTEENTRY glDirectDrawPalette::GetPalette(DWORD *flags)
{
	TRACE_ENTER(2,14,this,14,flags);
	if(flags)
	{
		*flags = this->flags;
		TRACE_VAR("*flags",9,*flags);
	}
	TRACE_RET(14,palette);
	return palette;
}
