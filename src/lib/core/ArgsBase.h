/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2012 Nick Bolton
 * 
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 * 
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "base/String.h"

class ArgsBase {
public:
    ArgsBase();
    virtual ~ArgsBase();

public:
    bool                m_daemon;
    bool                m_backend;
    bool                m_restartable;
    bool                m_noHooks;
    const char*            m_pname;
    const char*            m_logFilter;
    const char*            m_logFile;
    const char*            m_display;
    String                m_name;
    bool                m_enableDragDrop;
#if SYSAPI_WIN32
    bool                m_debugServiceWait;
    bool                m_pauseOnExit;
    bool                m_stopOnDeskSwitch;
#endif
#if WINAPI_XWINDOWS
    bool                m_disableXInitThreads;
    int                 m_runAsUid;
    bool                m_hid;
    String              m_keyboardDevice;
    String              m_mouseDevice;
    String              m_mouseAbsDevice;
    int                 m_screenWidth;
    int                 m_screenHeight;
    int                 m_screenX;
    int                 m_screenY;
#endif
    bool                m_shouldExit;
    String                m_synergyAddress;
    String                m_profileDirectory;
    String                m_pluginDirectory;
};
