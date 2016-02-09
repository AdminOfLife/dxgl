# DXGL 0.5.8
https://www.williamfeely.info/wiki/DXGL

## Introduction
DXGL is a free replacement for the Windows ddraw.dll library, running on OpenGL. It is designed to overcome driver bugs, particularly in Windows Vista and newer operating systems. It also adds various enhancements to the graphics output such as display scaling and filtering options. DXGL supports the DirectX 7.0 graphics APIs, however it is currently under development and does not work with many programs.

## GitHub Notice
If you downloaded DXGL from GitHub, please note that the version number will not work.

## System Requirements

* Windows XP SP3, Vista, 7, 8, 8.1, or 10 (May work under recent versions of Wine)
* OpenGL 2.0 or higher compatible video card, with hardware accelerated non-power-of-two size textures
  * OpenGL 3.2 or higher recommended.
* Visual C++ 2013 x86 runtime, available at https://www.microsoft.com/en-us/download/details.aspx?id=40784 (will be installed if not present)

## Build Requirements
* Visual Studio 2013, either full version or Express for Windows Desktop might work.
* The following components are optional.  The build process will ask for these if they do not exist:
  * TortoiseSVN (to fill in revision on SVN builds)
  * HTML Help Workshop (to build help)
  * NSIS (to build installer, requires TortoiseSVN and HTML Help Workshop to succeed)

## Build Instructions
These instructions assume that you do not have any of the required software installed. If you already have any or all of this software installed and set up, skip those steps.
* Install Visual Studio 2013 Community at https://www.visualstudio.com/en-us/news/vs2013-community-vs.aspx
* Open the dxgl.sln file, select your build configuration (Debug or Release) in the toolbar, and press F7 to build.

## Progress
For detailed progress information, please check https://www.williamfeely.info/wiki/DXGL_Features
What works:
* DirectDraw object creation and destruction (versions 1 to 7)
* Display mode enumeration and switching (with emulated mode switching)
* Fullscreen and windowed modes.
* Basic Blt() functionality
* 8-bit color emulated with GLSL shader

What partially works:
* 3D graphics are only partially supported.

What doesn't work:
* Many functions are stubbed out and return an error

## Installation

Run the installer.  When the installer completes, open DXGL Config and add your program files to the config program.
To uninstall, go to the Add/Remove Programs or Programs and Features control panel and uninstall.

## SVN

SVN readonly access is available at:
https://www.dxgl.info/svn/dxgl/

There is a Mediawiki-based SVN log at:
https://www.williamfeely.info/wiki/Special:Code/DXGL

## AppDB

An AppDB system (similar to that on winehq.org) is now available at:
https://www.dxgl.info/appdb/

This requires a user account separate from the other services.

Please note that the AppDB is now deprecated and will be made read-only once the new DXGL Wiki launches.

## Discussion boards

You may discuss DXGL at:
https://www.dxgl.info/phpBB3

You must create a forum account to post content.


## Bug reports

Bug reports are managed by a Bugzilla system available at:
https://www.dxgl.info/bugzilla

A user account needs to be created at this site to post bug reports.
