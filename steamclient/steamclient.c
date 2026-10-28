/*
 * steamclient64.dll: exports, interface lookup, configuration and the
 * callback queue.  The interface methods are in impl.c, the vtables in
 * the generated ifaces.c.
 *
 * steam_api64.dll loads this DLL from the path in
 * HKCU\Software\Valve\Steam\ActiveProcess\SteamClientDll64 (written by
 * steam.exe), calls CreateInterface("SteamClientNNN") and gets every
 * other interface through ISteamClient.  SteamAPI_RunCallbacks() pulls
 * callbacks with Steam_BGetCallback()/Steam_FreeLastCallback() and the
 * results of asynchronous calls with Steam_GetAPICallResult().
 *
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "steamclient.h"

struct config cfg;

static CRITICAL_SECTION lock;
static struct iface_obj *objs;
static int nobjs;
static int tracing;

void
trace(const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	int n;

	if (!tracing)
		return;
	n = snprintf(buf, sizeof(buf), "steamclient: ");
	va_start(ap, fmt);
	vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
	va_end(ap);
	OutputDebugStringA(buf);
	fprintf(stderr, "%s\n", buf);
}

/* ---- defaults used by the generated vtables ---- */

uint64_t d_zero(void) { return 0; }
void d_void(void) { }
float d_float(void) { return 0; }
double d_double(void) { return 0; }
const char *d_str(void) { return ""; }
void *d_ret2(void *self, void *ret) { (void)self; memset(ret, 0, 2); return ret; }
void *d_ret8(void *self, void *ret) { (void)self; memset(ret, 0, 8); return ret; }
void *d_ret16(void *self, void *ret) { (void)self; memset(ret, 0, 16); return ret; }
void *d_ret20(void *self, void *ret) { (void)self; memset(ret, 0, 20); return ret; }
void *d_ret40(void *self, void *ret) { (void)self; memset(ret, 0, 40); return ret; }

void *
get_iface(const char *version)
{
	int i;

	if (version == NULL)
		return NULL;
	for (i = 0; i < nobjs; i++)
		if (strcmp(objs[i].desc->version, version) == 0)
			return &objs[i];
	trace("no interface %s", version);
	return NULL;
}

void *iface_arg1(void *self, const char *v) { (void)self; return get_iface(v); }
void *iface_arg2(void *self, int32_t a, const char *v) { (void)self; (void)a; return get_iface(v); }
void *iface_arg3(void *self, int32_t a, int32_t b, const char *v) { (void)self; (void)a; (void)b; return get_iface(v); }

/* ---- configuration ---- */

/* Unix path from the environment to a Windows path ("Z:\..."). */
static void
dos_path(const char *unix_path, char *out, size_t n)
{
	WCHAR *(CDECL *to_dos)(const char *);
	WCHAR *w;

	out[0] = '\0';
	to_dos = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"),
	    "wine_get_dos_file_name");
	if (to_dos && (w = to_dos(unix_path))) {
		WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)n, NULL, NULL);
		HeapFree(GetProcessHeap(), 0, w);
	} else {
		size_t i;
		snprintf(out, n, "Z:%s", unix_path);
		for (i = 0; out[i]; i++)
			if (out[i] == '/')
				out[i] = '\\';
	}
}

static char *
env(const char *name)
{
	static char buf[65536];
	DWORD n;

	n = GetEnvironmentVariableA(name, buf, sizeof(buf));
	if (n == 0 || n >= sizeof(buf))
		return NULL;
	return buf;
}

static void
parse_ids(const char *s, uint32_t **ids, size_t *n)
{
	char *end;

	while (s && *s) {
		unsigned long v = strtoul(s, &end, 10);
		if (end == s)
			break;
		*ids = realloc(*ids, (*n + 1) * sizeof(**ids));
		(*ids)[(*n)++] = (uint32_t)v;
		s = *end ? end + 1 : end;
	}
}

/* PROTON_STEAM_DLC: "appid=name;appid=name" */
static void
parse_dlc(const char *s)
{
	while (s && *s) {
		const char *semi = strchr(s, ';'), *eq;
		size_t len = semi ? (size_t)(semi - s) : strlen(s);
		char *item = malloc(len + 1);

		memcpy(item, s, len);
		item[len] = '\0';
		eq = strchr(item, '=');
		cfg.dlc = realloc(cfg.dlc, (cfg.ndlc + 1) * sizeof(*cfg.dlc));
		cfg.dlcname = realloc(cfg.dlcname,
		    (cfg.ndlc + 1) * sizeof(*cfg.dlcname));
		cfg.dlc[cfg.ndlc] = strtoul(item, NULL, 10);
		cfg.dlcname[cfg.ndlc] = strdup(eq ? eq + 1 : "");
		cfg.ndlc++;
		free(item);
		s = semi ? semi + 1 : s + len;
	}
}

void
config_load(void)
{
	char *s;

	memset(&cfg, 0, sizeof(cfg));
	tracing = (s = env("PROTON_STEAM_TRACE")) && *s && *s != '0';
	if ((s = env("SteamAppId")) || (s = env("SteamGameId")))
		cfg.appid = strtoul(s, NULL, 10);
	/* An individual account in the public universe; account id 1 if
	 * proton-launcher has not logged in yet. */
	cfg.steamid = 0x0110000100000001ULL;
	if ((s = env("PROTON_STEAM_ID")))
		cfg.steamid = strtoull(s, NULL, 10);
	snprintf(cfg.name, sizeof(cfg.name), "%s",
	    (s = env("PROTON_STEAM_NAME")) ? s : "Player");
	snprintf(cfg.language, sizeof(cfg.language), "%s",
	    (s = env("PROTON_STEAM_LANGUAGE")) ? s : "english");
	if ((s = env("PROTON_STEAM_APPS")))
		parse_ids(s, &cfg.apps, &cfg.napps);
	if ((s = env("PROTON_STEAM_DLC")))
		parse_dlc(s);
	if ((s = env("PROTON_STEAM_BUILDID")))
		cfg.buildid = atoi(s);
	if ((s = env("PROTON_STEAM_GAMEDIR")))
		dos_path(s, cfg.gamedir, sizeof(cfg.gamedir));
	if ((s = env("PROTON_STEAM_USERDATA")))
		dos_path(s, cfg.userdir, sizeof(cfg.userdir));
	else {
		char pf[MAX_PATH] = "C:\\Program Files (x86)";
		GetEnvironmentVariableA("ProgramFiles(x86)", pf, sizeof(pf));
		snprintf(cfg.userdir, sizeof(cfg.userdir),
		    "%s\\Steam\\userdata\\%u\\%u", pf,
		    (unsigned)(cfg.steamid & 0xffffffff), cfg.appid);
	}
	trace("appid %u steamid %llu name \"%s\" %zu apps %zu dlc userdata %s",
	    cfg.appid, (unsigned long long)cfg.steamid, cfg.name, cfg.napps,
	    cfg.ndlc, cfg.userdir);
}

int
owns_app(uint32_t appid)
{
	size_t i;

	if (appid == cfg.appid && appid != 0)
		return 1;
	for (i = 0; i < cfg.napps; i++)
		if (cfg.apps[i] == appid)
			return 1;
	for (i = 0; i < cfg.ndlc; i++)
		if (cfg.dlc[i] == appid)
			return 1;
	return 0;
}

/* ---- callbacks ---- */

struct cb {
	int		 id;
	int		 size;
	unsigned char	*data;
	uint64_t	 call;		/* 0 for a plain callback */
	int		 failed;
	struct cb	*next;
};

/* posted, not yet handed out; call results wait in "results" after
 * their SteamAPICallCompleted_t has been delivered */
static struct cb *queue, **queue_tail = &queue, *results, *current;
static uint64_t next_call = 1;

#pragma pack(push, 8)
struct callback_msg {
	int32_t		 user;
	int32_t		 id;
	unsigned char	*param;
	int32_t		 size;
};

struct api_call_completed {
	uint64_t	 call;
	int32_t		 id;
	uint32_t	 size;
};
#pragma pack(pop)

static struct cb *
new_cb(int id, const void *data, int size)
{
	struct cb *c = calloc(1, sizeof(*c));

	c->id = id;
	c->size = size;
	c->data = malloc(size > 0 ? size : 1);
	if (size > 0)
		memcpy(c->data, data, size);
	return c;
}

static void
enqueue(struct cb *c)
{
	EnterCriticalSection(&lock);
	*queue_tail = c;
	queue_tail = &c->next;
	LeaveCriticalSection(&lock);
}

void
post_callback(int id, const void *data, int size)
{
	trace("callback %d", id);
	enqueue(new_cb(id, data, size));
}

uint64_t
post_result(int id, const void *data, int size, int failed)
{
	struct cb *c = new_cb(id, data, size);

	EnterCriticalSection(&lock);
	c->call = next_call++;
	c->failed = failed;
	LeaveCriticalSection(&lock);
	trace("call %llu result %d", (unsigned long long)c->call, id);
	enqueue(c);
	return c->call;
}

/* A result whose first field is its own call handle. */
uint64_t
post_result_self(int id, void *data, int size)
{
	struct cb *c;

	EnterCriticalSection(&lock);
	*(uint64_t *)data = next_call;
	c = new_cb(id, data, size);
	c->call = next_call++;
	LeaveCriticalSection(&lock);
	enqueue(c);
	return c->call;
}

static struct cb *
find_call(uint64_t call, struct cb ***prevp)
{
	struct cb **p;

	for (p = &results; *p; p = &(*p)->next)
		if ((*p)->call == call) {
			*prevp = p;
			return *p;
		}
	for (p = &queue; *p; p = &(*p)->next)
		if ((*p)->call == call) {
			*prevp = p;
			return *p;
		}
	return NULL;
}

int
call_completed(uint64_t call, int8_t *failed)
{
	struct cb **p, *c;
	int ok;

	EnterCriticalSection(&lock);
	if (current && current->call == call)
		c = current;
	else
		c = find_call(call, &p);
	ok = c != NULL;
	if (failed)
		*failed = c ? (int8_t)c->failed : 0;
	LeaveCriticalSection(&lock);
	return ok;
}

int
get_call_result(uint64_t call, void *out, int size, int expected,
    int8_t *failed)
{
	struct cb **p, *c;

	EnterCriticalSection(&lock);
	if (current && current->call == call) {
		/* asked while its SteamAPICallCompleted_t is being handled:
		 * hand it out; Steam_FreeLastCallback() frees it */
		c = current;
		memset(out, 0, size);
		memcpy(out, c->data, c->size < size ? c->size : size);
		if (failed)
			*failed = (int8_t)c->failed;
		c->call = 0;
		LeaveCriticalSection(&lock);
		return 1;
	}
	if ((c = find_call(call, &p)) == NULL) {
		LeaveCriticalSection(&lock);
		if (failed)
			*failed = 1;
		return 0;
	}
	if (expected && c->id != expected)
		trace("call %llu: result %d, caller expects %d",
		    (unsigned long long)call, c->id, expected);
	memset(out, 0, size);
	memcpy(out, c->data, c->size < size ? c->size : size);
	if (failed)
		*failed = (int8_t)c->failed;
	*p = c->next;
	if (queue_tail == &c->next)
		queue_tail = p;
	LeaveCriticalSection(&lock);
	free(c->data);
	free(c);
	return 1;
}

__declspec(dllexport) int8_t
Steam_BGetCallback(int32_t pipe, struct callback_msg *msg, int32_t *unused)
{
	static struct api_call_completed done;
	struct cb *c;

	(void)pipe;
	(void)unused;
	EnterCriticalSection(&lock);
	if (current || (c = queue) == NULL) {
		LeaveCriticalSection(&lock);
		return 0;
	}
	queue = c->next;
	if (queue == NULL)
		queue_tail = &queue;
	c->next = NULL;
	current = c;
	msg->user = 1;
	if (c->call) {
		done.call = c->call;
		done.id = c->id;
		done.size = c->size;
		msg->id = CB_STEAM_API_CALL_COMPLETED;
		msg->param = (unsigned char *)&done;
		msg->size = sizeof(done);
	} else {
		msg->id = c->id;
		msg->param = c->data;
		msg->size = c->size;
	}
	LeaveCriticalSection(&lock);
	return 1;
}

__declspec(dllexport) int8_t
Steam_FreeLastCallback(int32_t pipe)
{
	struct cb *c;

	(void)pipe;
	EnterCriticalSection(&lock);
	if ((c = current) == NULL) {
		LeaveCriticalSection(&lock);
		return 0;
	}
	current = NULL;
	if (c->call) {
		/* keep it for Steam_GetAPICallResult() */
		c->next = results;
		results = c;
		c = NULL;
	}
	LeaveCriticalSection(&lock);
	if (c) {
		free(c->data);
		free(c);
	}
	return 1;
}

__declspec(dllexport) int8_t
Steam_GetAPICallResult(int32_t pipe, uint64_t call, void *out, int size,
    int expected, int8_t *failed)
{
	(void)pipe;
	return (int8_t)get_call_result(call, out, size, expected, failed);
}

/* ---- other exports ---- */

__declspec(dllexport) void *
CreateInterface(const char *name, int *rc)
{
	void *p = get_iface(name);

	trace("CreateInterface %s -> %p", name, p);
	if (rc)
		*rc = p ? 0 : 1;
	return p;
}

__declspec(dllexport) int8_t
Steam_IsKnownInterface(const char *version)
{
	int i;

	for (i = 0; i < nobjs; i++)
		if (strcmp(objs[i].desc->version, version) == 0)
			return 1;
	return 0;
}

__declspec(dllexport) void
Steam_NotifyMissingInterface(int32_t pipe, const char *version)
{
	(void)pipe;
	trace("game asked for missing interface %s", version);
}

__declspec(dllexport) void Steam_ReleaseThreadLocalMemory(int exiting) { (void)exiting; }
__declspec(dllexport) void Breakpad_SteamMiniDumpInit(uint32_t a, const char *b, const char *c) { (void)a; (void)b; (void)c; }
__declspec(dllexport) void Breakpad_SteamSetAppID(uint32_t a) { (void)a; }
__declspec(dllexport) int Breakpad_SteamSetSteamID(uint64_t a) { (void)a; return 1; }
__declspec(dllexport) int Breakpad_SteamWriteMiniDumpSetComment(const char *a) { (void)a; return 1; }
__declspec(dllexport) void Breakpad_SteamWriteMiniDumpUsingExceptionInfoWithBuildId(int a, int b) { (void)a; (void)b; }

BOOL WINAPI
DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
	int i;

	(void)reserved;
	if (reason != DLL_PROCESS_ATTACH)
		return TRUE;
	DisableThreadLibraryCalls(inst);
	InitializeCriticalSection(&lock);
	for (nobjs = 0; iface_table[nobjs].version; nobjs++)
		;
	objs = calloc(nobjs, sizeof(*objs));
	for (i = 0; i < nobjs; i++) {
		objs[i].vtbl = iface_table[i].vtbl;
		objs[i].desc = &iface_table[i];
	}
	config_load();
	return TRUE;
}
