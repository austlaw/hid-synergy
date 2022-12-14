/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2002 Chris Schoeneman
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

#include "core/ClientApp.h"

#include "arch/Arch.h"
#include "arch/IArchTaskBarReceiver.h"
#include "base/Event.h"
#include "base/EventQueue.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "base/String.h"
#include "base/TMethodEventJob.h"
#include "base/TMethodJob.h"
#include "base/log_outputters.h"
#include "client/Client.h"
#include "common/Version.h"
#include "core/ArgParser.h"
#include "core/ClientArgs.h"
#include "core/Screen.h"
#include "core/XScreen.h"
#include "core/protocol_types.h"
#include "mt/Thread.h"
#include "net/NetworkAddress.h"
#include "net/SocketMultiplexer.h"
#include "net/TCPSocketFactory.h"
#include "net/XSocket.h"

#if SYSAPI_WIN32
#include "arch/win32/ArchMiscWindows.h"
#endif

#if WINAPI_MSWINDOWS
#include "platform/MSWindowsScreen.h"
#elif WINAPI_XWINDOWS
#include "platform/XWindowsScreen.h"
#include "platform/HIDScreen.h"
#elif WINAPI_CARBON
#include "platform/OSXScreen.h"
#endif

#if defined(__APPLE__)
#include "platform/OSXDragSimulator.h"
#endif

#include <iostream>
#include <cstdio>

#define RETRY_TIME 1.0

ClientApp::ClientApp(IEventQueue* events) :
    App(events, new ClientArgs()),
    m_client(nullptr),
    m_clientScreen(nullptr),
    m_serverAddress(nullptr)
{
}

ClientApp::~ClientApp()
= default;

void
ClientApp::parseArgs(int argc, const char* const* argv)
{
    ArgParser argParser(this);
    bool result = argParser.parseClientArgs(args(), argc, argv);

    if (!result || args().m_shouldExit) {
        m_bye(kExitArgs);
    }
    else {
        // save server address
        if (!args().m_synergyAddress.empty()) {
            try {
                *m_serverAddress = NetworkAddress(args().m_synergyAddress, kDefaultPort);
                m_serverAddress->resolve();
            }
            catch (XSocketAddress& e) {
                // allow an address that we can't look up if we're restartable.
                // we'll try to resolve the address each time we connect to the
                // server.  a bad port will never get better.  patch by Brent
                // Priddy.
                if (!args().m_restartable || e.getError() == XSocketAddress::kBadPort) {
                    LOG((CLOG_PRINT "%s: %s" BYE,
                        args().m_pname, e.what(), args().m_pname));
                    m_bye(kExitFailed);
                }
            }
        }
    }
}

void
ClientApp::help()
{
#if WINAPI_XWINDOWS
#  define WINAPI_ARG \
    " [--display <display>] [--no-xinitthreads] [--hid <keyboard> <relative mouse> <absolute mouse> <width> <height> <x> <y>]"
#  define WINAPI_INFO \
    "      --display <display>  connect to the X server at <display>\n" \
    "      --no-xinitthreads    do not call XInitThreads()\n" \
    "      --hid <keyboard> <relative mouse> <absolute mouse> <width> <height>\n" \
    "        run in HID mode with the given <keyboard> and <mouse> devices\n"
#else
#  define WINAPI_ARG
#  define WINAPI_INFO
#endif

    char buffer[2000];
    sprintf(
        buffer,
        "Usage: %s"
        " [--yscroll <delta>]"
        WINAPI_ARG
        HELP_SYS_ARGS
        HELP_COMMON_ARGS
        " <server-address>"
        "\n\n"
        "Connect to a synergy mouse/keyboard sharing server.\n"
        "\n"
        HELP_COMMON_INFO_1
        WINAPI_INFO
        HELP_SYS_INFO
        "      --yscroll <delta>    defines the vertical scrolling delta, which is\n"
        "                             120 by default.\n"
        HELP_COMMON_INFO_2
        "\n"
        "* marks defaults.\n"
        "\n"
        "The server address is of the form: [<hostname>][:<port>].  The hostname\n"
        "must be the address or hostname of the server.  The port overrides the\n"
        "default port, %d.\n",
        args().m_pname, kDefaultPort
    );

    LOG((CLOG_PRINT "%s", buffer));
}

const char*
ClientApp::daemonName() const
{
#if SYSAPI_WIN32
    return "Synergy Client";
#elif SYSAPI_UNIX
    return "synergyc";
#endif
}

const char*
ClientApp::daemonInfo() const
{
#if SYSAPI_WIN32
    return "Allows another computer to share it's keyboard and mouse with this computer.";
#elif SYSAPI_UNIX
    return "";
#endif
}

synergy::Screen*
ClientApp::createScreen()
{
#if WINAPI_MSWINDOWS
    return new synergy::Screen(new MSWindowsScreen(
        false, args().m_noHooks, args().m_stopOnDeskSwitch, m_events), m_events);
#elif WINAPI_XWINDOWS
    if (args().m_hid) {
        return new synergy::Screen(new HIDScreen(
                args().m_keyboardDevice,
                args().m_mouseDevice,
                args().m_mouseAbsDevice,
                args().m_screenWidth, args().m_screenHeight,
                args().m_screenX, args().m_screenY,
                m_events), m_events);
        //m_bye(kExitArgs);
    }
    else {
        return new synergy::Screen(new XWindowsScreen(
                args().m_display, false, args().m_disableXInitThreads,
                args().m_yscroll, m_events), m_events);
    }
#elif WINAPI_CARBON
    return new synergy::Screen(new OSXScreen(m_events, false), m_events);
#endif
}

void
ClientApp::updateStatus()
{
    updateStatus("");
}


void
ClientApp::updateStatus(const String&  /*msg*/)
{
}


void
ClientApp::resetRestartTimeout()
{
    // retry time can nolonger be changed
    //s_retryTime = 0.0;
}


double
ClientApp::nextRestartTimeout()
{
    // retry at a constant rate (Issue 52)
    return RETRY_TIME;

    /*
    // choose next restart timeout.  we start with rapid retries
    // then slow down.
    if (s_retryTime < 1.0) {
    s_retryTime = 1.0;
    }
    else if (s_retryTime < 3.0) {
    s_retryTime = 3.0;
    }
    else {
    s_retryTime = 5.0;
    }
    return s_retryTime;
    */
}


void
ClientApp::handleScreenError(const Event& /*unused*/, void* /*unused*/)
{
    LOG((CLOG_CRIT "error on screen"));
    m_events->addEvent(Event(Event::kQuit));
}


synergy::Screen*
ClientApp::openClientScreen()
{
    synergy::Screen* screen = createScreen();
    screen->setEnableDragDrop(argsBase().m_enableDragDrop);
    m_events->adoptHandler(m_events->forIScreen().error(),
        screen->getEventTarget(),
        new TMethodEventJob<ClientApp>(
        this, &ClientApp::handleScreenError));
    return screen;
}


void
ClientApp::closeClientScreen(synergy::Screen* screen)
{
    if (screen != nullptr) {
        m_events->removeHandler(m_events->forIScreen().error(),
            screen->getEventTarget());
        delete screen;
    }
}


void
ClientApp::handleClientRestart(const Event& /*unused*/, void* vtimer)
{
    // discard old timer
    auto* timer = static_cast<EventQueueTimer*>(vtimer);
    m_events->deleteTimer(timer);
    m_events->removeHandler(Event::kTimer, timer);

    // reconnect
    startClient();
}


void
ClientApp::scheduleClientRestart(double retryTime)
{
    // install a timer and handler to retry later
    LOG((CLOG_DEBUG "retry in %.0f seconds", retryTime));
    EventQueueTimer* timer = m_events->newOneShotTimer(retryTime, nullptr);
    m_events->adoptHandler(Event::kTimer, timer,
        new TMethodEventJob<ClientApp>(this, &ClientApp::handleClientRestart, timer));
}


void
ClientApp::handleClientConnected(const Event& /*unused*/, void* /*unused*/)
{
    LOG((CLOG_NOTE "connected to server"));
    resetRestartTimeout();
    updateStatus();
}


void
ClientApp::handleClientFailed(const Event& e, void* /*unused*/)
{
    auto* info =
        static_cast<Client::FailInfo*>(e.getData());

    updateStatus(String("Failed to connect to server: ") + info->m_what);
    if (!args().m_restartable || !info->m_retry) {
        LOG((CLOG_ERR "failed to connect to server: %s", info->m_what.c_str()));
        m_events->addEvent(Event(Event::kQuit));
    }
    else {
        LOG((CLOG_WARN "failed to connect to server: %s", info->m_what.c_str()));
        if (!m_suspended) {
            scheduleClientRestart(nextRestartTimeout());
        }
    }
    delete info;
}


void
ClientApp::handleClientDisconnected(const Event& /*unused*/, void* /*unused*/)
{
    LOG((CLOG_NOTE "disconnected from server"));
    if (!args().m_restartable) {
        m_events->addEvent(Event(Event::kQuit));
    }
    else if (!m_suspended) {
        scheduleClientRestart(nextRestartTimeout());
    }
    updateStatus();
}

Client*
ClientApp::openClient(const String& name, const NetworkAddress& address,
                synergy::Screen* screen)
{
    auto* client = new Client(
        m_events,
        name,
        address,
        new TCPSocketFactory(m_events, getSocketMultiplexer()),
        screen,
        args());

    try {
        m_events->adoptHandler(
            m_events->forClient().connected(),
            client->getEventTarget(),
            new TMethodEventJob<ClientApp>(this, &ClientApp::handleClientConnected));

        m_events->adoptHandler(
            m_events->forClient().connectionFailed(),
            client->getEventTarget(),
            new TMethodEventJob<ClientApp>(this, &ClientApp::handleClientFailed));

        m_events->adoptHandler(
            m_events->forClient().disconnected(),
            client->getEventTarget(),
            new TMethodEventJob<ClientApp>(this, &ClientApp::handleClientDisconnected));

    } catch (std::bad_alloc &ba) {
        delete client;
        throw ba;
    }

    return client;
}


void
ClientApp::closeClient(Client* client)
{
    if (client == nullptr) {
        return;
    }

    m_events->removeHandler(m_events->forClient().connected(), client);
    m_events->removeHandler(m_events->forClient().connectionFailed(), client);
    m_events->removeHandler(m_events->forClient().disconnected(), client);
    delete client;
}

int
ClientApp::foregroundStartup(int argc, char** argv)
{
    initApp(argc, argv);

    // never daemonize
    return mainLoop();
}

bool
ClientApp::startClient()
{
    double retryTime;
    synergy::Screen* clientScreen = nullptr;
    try {
        if (m_clientScreen == nullptr) {
            clientScreen = openClientScreen();
            m_client     = openClient(args().m_name,
                *m_serverAddress, clientScreen);
            m_clientScreen  = clientScreen;
            LOG((CLOG_NOTE "started client"));
        }

        m_client->connect();

        updateStatus();
        return true;
    }
    catch (XScreenUnavailable& e) {
        LOG((CLOG_WARN "secondary screen unavailable: %s", e.what()));
        closeClientScreen(clientScreen);
        updateStatus(String("secondary screen unavailable: ") + e.what());
        retryTime = e.getRetryTime();
    }
    catch (XScreenOpenFailure& e) {
        LOG((CLOG_CRIT "failed to start client: %s", e.what()));
        closeClientScreen(clientScreen);
        return false;
    }
    catch (XBase& e) {
        LOG((CLOG_CRIT "failed to start client: %s", e.what()));
        closeClientScreen(clientScreen);
        return false;
    }

    if (args().m_restartable) {
        scheduleClientRestart(retryTime);
        return true;
    }
    
        // don't try again
        return false;
    
}


void
ClientApp::stopClient()
{
    closeClient(m_client);
    closeClientScreen(m_clientScreen);
    m_client       = nullptr;
    m_clientScreen = nullptr;
}


int
ClientApp::mainLoop()
{
    // create socket multiplexer.  this must happen after daemonization
    // on unix because threads evaporate across a fork().
    SocketMultiplexer multiplexer;
    setSocketMultiplexer(&multiplexer);

    // start client, etc
    appUtil().startNode();
    
    // run event loop.  if startClient() failed we're supposed to retry
    // later.  the timer installed by startClient() will take care of
    // that.
    DAEMON_RUNNING(true);
    
#if defined(MAC_OS_X_VERSION_10_7)
    
    Thread thread(
        new TMethodJob<ClientApp>(
            this, &ClientApp::runEventsLoop,
            NULL));
    
    // wait until carbon loop is ready
    OSXScreen* screen = dynamic_cast<OSXScreen*>(
        m_clientScreen->getPlatformScreen());
    screen->waitForCarbonLoop();
    
    runCocoaApp();
#else
    m_events->loop();
#endif
    
    DAEMON_RUNNING(false);

    // close down
    LOG((CLOG_DEBUG1 "stopping client"));
    stopClient();
    updateStatus();
    LOG((CLOG_NOTE "stopped client"));

    return kExitSuccess;
}

static
int
daemonMainLoopStatic(int argc, const char** argv)
{
    return ClientApp::instance().daemonMainLoop(argc, argv);
}

int
ClientApp::standardStartup(int argc, char** argv)
{
    initApp(argc, argv);

    // daemonize if requested
    if (args().m_daemon) {
        return ARCH->daemonize(daemonName(), &daemonMainLoopStatic);
    }
    
        return mainLoop();
    
}

int
ClientApp::runInner(int argc, char** argv, ILogOutputter* outputter, StartupFunc startup)
{
    // general initialization
    m_serverAddress = new NetworkAddress;
    args().m_pname         = ARCH->getBasename(argv[0]);

    // install caller's output filter
    if (outputter != nullptr) {
        CLOG->insert(outputter);
    }

    int result;
    try
    {
        // run
        result = startup(argc, argv);
    }
    catch (...)
    {
        delete m_serverAddress;

        throw;
    }

    return result;
}

void 
ClientApp::startNode()
{
    // start the client.  if this return false then we've failed and
    // we shouldn't retry.
    LOG((CLOG_DEBUG1 "starting client"));
    if (!startClient()) {
        m_bye(kExitFailed);
    }
}
