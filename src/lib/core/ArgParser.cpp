/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2014-2016 Symless Ltd.
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

#include "core/ArgParser.h"

#include "base/Log.h"
#include "base/String.h"
#include "core/App.h"
#include "core/ArgsBase.h"
#include "core/ClientArgs.h"
#include "core/ServerArgs.h"
#include "core/StreamChunker.h"
#include "core/ToolArgs.h"

#ifdef WINAPI_MSWINDOWS
#include <VersionHelpers.h>
#endif
 
ArgsBase* ArgParser::m_argsBase = nullptr;

ArgParser::ArgParser(App* app) :
    m_app(app)
{
}

bool
ArgParser::parseServerArgs(ServerArgs& args, int argc, const char* const* argv)
{
    setArgsBase(args);
    updateCommonArgs(argv);

    for (int i = 1; i < argc; ++i) {
        if (parsePlatformArg(args, argc, argv, i)) {
            continue;
        }
        if (parseGenericArgs(argc, argv, i)) {
            continue;
        }
        else if (parseDeprecatedArgs(argc, argv, i)) {
            continue;
        }
        else if (isArg(i, argc, argv, "-a", "--address", 1)) {
            // save listen address
            args.m_synergyAddress = argv[++i];
        }
        else if (isArg(i, argc, argv, "-c", "--config", 1)) {
            // save configuration file path
            args.m_configFile = argv[++i];
        }
        else if (isArg(i, argc, argv, "", "--serial-key", 1)) {
            args.m_serial = SerialKey(argv[++i]);
        }
        else {
            LOG((CLOG_PRINT "%s: unrecognized option `%s'" BYE, args.m_pname, argv[i], args.m_pname));
            return false;
        }
    }

    return !checkUnexpectedArgs();
}

bool
ArgParser::parseClientArgs(ClientArgs& args, int argc, const char* const* argv)
{
    setArgsBase(args);
    updateCommonArgs(argv);

    int i;
    for (i = 1; i < argc; ++i) {
        if (parsePlatformArg(args, argc, argv, i)) {
            continue;
        }
        if (parseGenericArgs(argc, argv, i)) {
            continue;
        }
        else if (parseDeprecatedArgs(argc, argv, i)) {
            continue;
        }
        else if (isArg(i, argc, argv, nullptr, "--camp")) {
            // ignore -- included for backwards compatibility
        }
        else if (isArg(i, argc, argv, nullptr, "--no-camp")) {
            // ignore -- included for backwards compatibility
        }
        else if (isArg(i, argc, argv, nullptr, "--yscroll", 1)) {
            // define scroll
            args.m_yscroll = atoi(argv[++i]);
        }
        else {
            if (i + 1 == argc) {
                args.m_synergyAddress = argv[i];
                return true;
            }

            LOG((CLOG_PRINT "%s: unrecognized option `%s'" BYE, args.m_pname, argv[i], args.m_pname));
            return false;
        }
    }

    // exactly one non-option argument (server-address)
    if (i == argc) {
        LOG((CLOG_PRINT "%s: a server address or name is required" BYE,
            args.m_pname, args.m_pname));
        return false;
    }

    return !checkUnexpectedArgs();
}

bool
ArgParser::parsePlatformArg(ArgsBase& argsBase, const int& argc, const char* const* argv, int& i)
{
#if WINAPI_MSWINDOWS
    if (isArg(i, argc, argv, NULL, "--service")) {
        LOG((CLOG_WARN "obsolete argument --service, use synergyd instead."));
        argsBase.m_shouldExit = true;
    }
    else if (isArg(i, argc, argv, NULL, "--exit-pause")) {
        argsBase.m_pauseOnExit = true;
    }
    else if (isArg(i, argc, argv, NULL, "--stop-on-desk-switch")) {
        argsBase.m_stopOnDeskSwitch = true;
    }
    else {
        // option not supported here
        return false;
    }

    return true;
#elif WINAPI_XWINDOWS
    if (isArg(i, argc, argv, "-display", "--display", 1)) {
        // use alternative display
        argsBase.m_display = argv[++i];
    }

    else if (isArg(i, argc, argv, nullptr, "--no-xinitthreads")) {
        argsBase.m_disableXInitThreads = true;
    }

    else if (isArg(i, argc, argv, nullptr, "--hid", 7)) {
        argsBase.m_hid = true;
        argsBase.m_keyboardDevice = argv[++i];
        argsBase.m_mouseDevice = argv[++i];
        argsBase.m_mouseAbsDevice = argv[++i];
        argsBase.m_screenWidth = atoi(argv[++i]);
        argsBase.m_screenHeight = atoi(argv[++i]);
        argsBase.m_screenX = atoi(argv[++i]);
        argsBase.m_screenY = atoi(argv[++i]);
    }

    else {
        // option not supported here
        return false;
    }

    return true;
#elif WINAPI_CARBON
    // no options for carbon
    return false;
#endif
}

bool
ArgParser::parseToolArgs(ToolArgs& args, int argc, const char* const* argv)
{
    for (int i = 1; i < argc; ++i) {
        if (isArg(i, argc, argv, nullptr, "--get-active-desktop", 0)) {
            args.m_printActiveDesktopName = true;
            return true;
        }
        if (isArg(i, argc, argv, nullptr, "--login-auth", 0)) {
            args.m_loginAuthenticate = true;
            return true;
        }
        else if (isArg(i, argc, argv, nullptr, "--get-installed-dir", 0)) {
            args.m_getInstalledDir = true;
            return true;
        }
        else if (isArg(i, argc, argv, nullptr, "--get-profile-dir", 0)) {
            args.m_getProfileDir = true;
            return true;
        }
        else if (isArg(i, argc, argv, nullptr, "--get-arch", 0)) {
            args.m_getArch = true;
            return true;
        }
        else if (isArg(i, argc, argv, nullptr, "--notify-activation", 0)) {
            args.m_notifyActivation = true;
            return true;
        }
        else if (isArg(i, argc, argv, nullptr, "--notify-update", 0)) {
            args.m_notifyUpdate = true;
            return true;
        }
        else {
            return false;
        }
    }

    return false;
}

bool
ArgParser::parseGenericArgs(int argc, const char* const* argv, int& i)
{
    if (isArg(i, argc, argv, "-d", "--debug", 1)) {
        // change logging level
        argsBase().m_logFilter = argv[++i];
    }
    else if (isArg(i, argc, argv, "-l", "--log", 1)) {
        argsBase().m_logFile = argv[++i];
    }
    else if (isArg(i, argc, argv, "-f", "--no-daemon")) {
        // not a daemon
        argsBase().m_daemon = false;
    }
    else if (isArg(i, argc, argv, nullptr, "--daemon")) {
        // daemonize
        argsBase().m_daemon = true;
    }
    else if (isArg(i, argc, argv, "-n", "--name", 1)) {
        // save screen name
        argsBase().m_name = argv[++i];
    }
    else if (isArg(i, argc, argv, "-1", "--no-restart")) {
        // don't try to restart
        argsBase().m_restartable = false;
    }
    else if (isArg(i, argc, argv, nullptr, "--restart")) {
        // try to restart
        argsBase().m_restartable = true;
    }
    else if (isArg(i, argc, argv, "-z", nullptr)) {
        argsBase().m_backend = true;
    }
    else if (isArg(i, argc, argv, nullptr, "--no-hooks")) {
        argsBase().m_noHooks = true;
    }
    else if (isArg(i, argc, argv, "-h", "--help")) {
        if (m_app != nullptr) {
            m_app->help();
        }
        argsBase().m_shouldExit = true;
    }
    else if (isArg(i, argc, argv, nullptr, "--version")) {
        if (m_app != nullptr) {
            m_app->version();
        }
        argsBase().m_shouldExit = true;
    }
    else if (isArg(i, argc, argv, nullptr, "--ipc")) {
        LOG((CLOG_INFO "ignoring --ipc. The old IPC was removed."));
    }
    else if (isArg(i, argc, argv, nullptr, "--server")) {
        // supress error when --server is used
    }
    else if (isArg(i, argc, argv, nullptr, "--client")) {
        // supress error when --client is used
    }
    else if (isArg(i, argc, argv, nullptr, "--enable-drag-drop")) {
        bool useDragDrop = true;

#ifdef WINAPI_XWINDOWS

        useDragDrop = false;
        LOG((CLOG_INFO "ignoring --enable-drag-drop, not supported on linux."));

#endif

#ifdef WINAPI_MSWINDOWS

        if (!IsWindowsVistaOrGreater()) {
            useDragDrop = false;
            LOG((CLOG_INFO "ignoring --enable-drag-drop, not supported below vista."));
        }
#endif

        if (useDragDrop) {
            argsBase().m_enableDragDrop = true;
        }
    }
    else if (isArg(i, argc, argv, nullptr, "--enable-crypto")) {
        LOG((CLOG_INFO "--enable-crypto ignored, TLS is no longer supported in Synergy Core"));
        return false;
    }
    else if (isArg(i, argc, argv, nullptr, "--profile-dir", 1)) {
        argsBase().m_profileDirectory = argv[++i];
    }
    else if (isArg(i, argc, argv, nullptr, "--plugin-dir", 1)) {
        argsBase().m_pluginDirectory = argv[++i];
    }
#if WINAPI_XWINDOWS
    else if (isArg(i, argc, argv, nullptr, "--run-as-uid", 1)) {
        argsBase().m_runAsUid = std::stoi(argv[++i]);
    }
#endif
    else {
        // option not supported here
        return false;
    }

    return true;
}

bool
ArgParser::parseDeprecatedArgs(int argc, const char* const* argv, int& i)
{
    if (isArg(i, argc, argv, nullptr, "--crypto-pass")) {
        LOG((CLOG_NOTE "--crypto-pass is deprecated"));
        i++;
        return true;
    }
    if (isArg(i, argc, argv, nullptr, "--res-w")) {
        LOG((CLOG_NOTE "--res-w is deprecated"));
        i++;
        return true;
    }
    else if (isArg(i, argc, argv, nullptr, "--res-h")) {
        LOG((CLOG_NOTE "--res-h is deprecated"));
        i++;
        return true;
    }
    else if (isArg(i, argc, argv, nullptr, "--prm-wc")) {
        LOG((CLOG_NOTE "--prm-wc is deprecated"));
        i++;
        return true;
    }
    else if (isArg(i, argc, argv, nullptr, "--prm-hc")) {
        LOG((CLOG_NOTE "--prm-hc is deprecated"));
        i++;
        return true;
    }

    return false;
}

bool
ArgParser::isArg(
    int argi, int argc, const char* const* argv,
    const char* name1, const char* name2,
    int minRequiredParameters)
{
    if ((name1 != nullptr && strcmp(argv[argi], name1) == 0) ||
        (name2 != nullptr && strcmp(argv[argi], name2) == 0)) {
            // match.  check args left.
            if (argi + minRequiredParameters >= argc) {
                LOG((CLOG_PRINT "%s: missing arguments for `%s'" BYE,
                    argsBase().m_pname, argv[argi], argsBase().m_pname));
                argsBase().m_shouldExit = true;
                return false;
            }
            return true;
    }

    // no match
    return false;
}

void
ArgParser::splitCommandString(String& command, std::vector<String>& argv)
{
    if (command.empty()) {
        return ;
    }

    size_t leftDoubleQuote = 0;
    size_t rightDoubleQuote = 0;
    searchDoubleQuotes(command, leftDoubleQuote, rightDoubleQuote);

    size_t startPos = 0;
    size_t space = command.find(' ', startPos);

    while (space != String::npos) {
        bool ignoreThisSpace = false;

        // check if the space is between two double quotes
        if (space > leftDoubleQuote && space < rightDoubleQuote) {
            ignoreThisSpace = true;
        }
        else if (space > rightDoubleQuote){
            searchDoubleQuotes(command, leftDoubleQuote, rightDoubleQuote, rightDoubleQuote + 1);
        }

        if (!ignoreThisSpace) {
            String subString = command.substr(startPos, space - startPos);

            removeDoubleQuotes(subString);
            argv.push_back(subString);
        }

        // find next space
        if (ignoreThisSpace) {
            space = command.find(' ', rightDoubleQuote + 1);
        }
        else {
            startPos = space + 1;
            space = command.find(' ', startPos);
        }
    }

    String subString = command.substr(startPos, command.size());
    removeDoubleQuotes(subString);
    argv.push_back(subString);
}

bool
ArgParser::searchDoubleQuotes(String& command, size_t& left, size_t& right, size_t startPos)
{
    bool result = false;
    left = String::npos;
    right = String::npos;

    left = command.find('\"', startPos);
    if (left != String::npos) {
        right = command.find('\"', left + 1);
        if (right != String::npos) {
            result = true;
        }
    }

    if (!result) {
        left = 0;
        right = 0;
    }

    return result;
}

void
ArgParser::removeDoubleQuotes(String& arg)
{
    // if string is surrounded by double quotes, remove them
    if (arg[0] == '\"' &&
        arg[arg.size() - 1] == '\"') {
        arg = arg.substr(1, arg.size() - 2);
    }
}

const char**
ArgParser::getArgv(std::vector<String>& argsArray)
{
    size_t argc = argsArray.size();

    // caller is responsible for deleting the outer array only
    // we use the c string pointers from argsArray and assign
    // them to the inner array. So caller only need to use
    // delete[] to delete the outer array
    const auto** argv = new const char*[argc];

    for (size_t i = 0; i < argc; i++) {
        argv[i] = argsArray[i].c_str();
    }

    return argv;
}

String
ArgParser::assembleCommand(std::vector<String>& argsArray,  const String& ignoreArg, int parametersRequired)
{
    String result;

    for (auto it = argsArray.begin(); it != argsArray.end(); ++it) {
        if (*it == ignoreArg) {
            it = it + parametersRequired;
            continue;
        }

        // if there is a space in this arg, use double quotes surround it
        if ((*it).find(" ") != String::npos) {
            (*it).insert(0, "\"");
            (*it).push_back('\"');
        }

        result.append(*it);
        // add space to saperate args
        result.append(" ");
    }

    if (!result.empty()) {
        // remove the tail space
        result = result.substr(0, result.size() - 1);
    }

    return result;
}

void
ArgParser::updateCommonArgs(const char* const* argv)
{
    argsBase().m_name = ARCH->getHostName();
    argsBase().m_pname = ARCH->getBasename(argv[0]);
}

bool
ArgParser::checkUnexpectedArgs()
{
#if SYSAPI_WIN32
    // suggest that user installs as a windows service. when launched as
    // service, process should automatically detect that it should run in
    // daemon mode.
    if (argsBase().m_daemon) {
        LOG((CLOG_ERR
            "the --daemon argument is not supported on windows. "
            "instead, install %s as a service (--service install)",
            argsBase().m_pname));
        return true;
    }
#endif

    return false;
}
