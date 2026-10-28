/*
 * steam.exe for proton-openbsd: stands in for the running Steam client
 * the way Proton's steam_helper does.  It records itself as the active
 * Steam process and steamclient64.dll as the client library in the
 * registry, where steam_api64.dll looks for them, then runs the game
 * given on its command line and exits with the game's exit code.
 *
 *	steam.exe game.exe [arg ...]
 *
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#ifndef CP_UNIXCP
#define CP_UNIXCP 65010		/* wine: the Unix locale's code page */
#endif

static void
set_str(HKEY key, const WCHAR *name, const WCHAR *value)
{
	RegSetValueExW(key, name, 0, REG_SZ, (const BYTE *)value,
	    (DWORD)((wcslen(value) + 1) * sizeof(WCHAR)));
}

static void
set_dword(HKEY key, const WCHAR *name, DWORD value)
{
	RegSetValueExW(key, name, 0, REG_DWORD, (const BYTE *)&value,
	    sizeof(value));
}

static HKEY
open_key(const WCHAR *path)
{
	HKEY key;

	if (RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, NULL, 0,
	    KEY_ALL_ACCESS, NULL, &key, NULL) != ERROR_SUCCESS)
		return NULL;
	return key;
}

static void
setup_registry(const WCHAR *dir)
{
	WCHAR buf[MAX_PATH + 64], lang[64] = L"english", app[64];
	DWORD appid = 0, account = 1;
	HKEY key;

	GetEnvironmentVariableW(L"PROTON_STEAM_LANGUAGE", lang, 64);
	if (GetEnvironmentVariableW(L"SteamAppId", app, 64))
		appid = wcstoul(app, NULL, 10);
	if (GetEnvironmentVariableW(L"PROTON_STEAM_ID", buf, 64))
		account = (DWORD)(_wcstoui64(buf, NULL, 10) & 0xffffffff);

	if ((key = open_key(L"Software\\Valve\\Steam"))) {
		set_str(key, L"SteamPath", dir);
		swprintf(buf, MAX_PATH + 64, L"%ls\\steam.exe", dir);
		set_str(key, L"SteamExe", buf);
		set_str(key, L"Language", lang);
		set_str(key, L"language", lang);
		RegCloseKey(key);
	}
	if ((key = open_key(L"Software\\Valve\\Steam\\ActiveProcess"))) {
		set_dword(key, L"pid", GetCurrentProcessId());
		set_dword(key, L"ActiveUser", account);
		swprintf(buf, MAX_PATH + 64, L"%ls\\steamclient64.dll", dir);
		set_str(key, L"SteamClientDll64", buf);
		swprintf(buf, MAX_PATH + 64, L"%ls\\steamclient.dll", dir);
		set_str(key, L"SteamClientDll", buf);
		set_str(key, L"Universe", L"Public");
		RegCloseKey(key);
	}
	if (appid) {
		swprintf(buf, MAX_PATH + 64, L"Software\\Valve\\Steam\\Apps\\%lu",
		    (unsigned long)appid);
		if ((key = open_key(buf))) {
			set_dword(key, L"Installed", 1);
			set_dword(key, L"Running", 1);
			set_dword(key, L"Updating", 0);
			RegCloseKey(key);
		}
	}
	SetEnvironmentVariableW(L"SteamPath", dir);
}

static void
clear_running(void)
{
	WCHAR app[64], buf[96];
	HKEY key;

	if ((key = open_key(L"Software\\Valve\\Steam\\ActiveProcess"))) {
		set_dword(key, L"pid", 0);
		RegCloseKey(key);
	}
	if (GetEnvironmentVariableW(L"SteamAppId", app, 64)) {
		swprintf(buf, 96, L"Software\\Valve\\Steam\\Apps\\%ls", app);
		if ((key = open_key(buf))) {
			set_dword(key, L"Running", 0);
			RegCloseKey(key);
		}
	}
}

/* The command line after our own program name. */
static WCHAR *
skip_program(WCHAR *cmd)
{
	int quoted = 0;

	while (*cmd && (quoted || (*cmd != L' ' && *cmd != L'\t'))) {
		if (*cmd == L'"')
			quoted = !quoted;
		cmd++;
	}
	while (*cmd == L' ' || *cmd == L'\t')
		cmd++;
	return cmd;
}

/*
 * proton-openbsd passes the game as a Unix path, which CreateProcess()
 * does not resolve (wine's own launcher converts it).  Convert the first
 * word of the command line if it is one.
 */
static WCHAR *
dos_command(WCHAR *cmd)
{
	WCHAR *(CDECL *to_dos)(const char *);
	WCHAR *end, *dos, *out;
	char unixpath[4096];
	size_t n;
	int len;

	if (*cmd == L'"') {
		if ((end = wcschr(cmd + 1, L'"')) == NULL)
			return cmd;
		n = end - cmd - 1;
		cmd++;
		end++;
	} else {
		for (end = cmd; *end && *end != L' ' && *end != L'\t'; end++)
			;
		n = end - cmd;
	}
	if (cmd[0] != L'/' || n >= sizeof(unixpath))
		return cmd[-1] == L'"' ? cmd - 1 : cmd;
	to_dos = (void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
	    "wine_get_dos_file_name");
	if (to_dos == NULL || (len = WideCharToMultiByte(CP_UNIXCP, 0, cmd,
	    (int)n, unixpath, sizeof(unixpath) - 1, NULL, NULL)) <= 0)
		return cmd[-1] == L'"' ? cmd - 1 : cmd;
	unixpath[len] = '\0';
	if ((dos = to_dos(unixpath)) == NULL)
		return cmd[-1] == L'"' ? cmd - 1 : cmd;
	n = wcslen(dos) + wcslen(end) + 3;
	out = malloc(n * sizeof(WCHAR));
	swprintf(out, n, L"\"%ls\"%ls", dos, end);
	return out;
}

int
wmain(void)
{
	WCHAR dir[MAX_PATH], *s, *cmd;
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	DWORD code = 1;

	GetModuleFileNameW(NULL, dir, MAX_PATH);
	if ((s = wcsrchr(dir, L'\\')))
		*s = L'\0';
	setup_registry(dir);
	/* objects Steam creates that some launchers look for */
	CreateEventW(NULL, FALSE, FALSE, L"Steam3Master_SharedMemLock");
	CreateEventW(NULL, FALSE, FALSE, L"Global\\Valve_SteamIPC_Class");

	cmd = skip_program(GetCommandLineW());
	if (*cmd == L'\0') {
		fprintf(stderr, "usage: steam.exe program.exe [arg ...]\n");
		return 2;
	}
	cmd = _wcsdup(dos_command(cmd));
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si,
	    &pi)) {
		fprintf(stderr, "steam.exe: cannot start %ls (error %lu)\n", cmd,
		    (unsigned long)GetLastError());
		clear_running();
		return 1;
	}
	WaitForSingleObject(pi.hProcess, INFINITE);
	GetExitCodeProcess(pi.hProcess, &code);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	clear_running();
	return (int)code;
}
