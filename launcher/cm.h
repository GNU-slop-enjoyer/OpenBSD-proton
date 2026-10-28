/*
 * A client for Steam's connection managers (CM) over WebSocket: the same
 * message protocol the Steam client speaks, documented by the open-source
 * SteamKit2 and SteamDatabase/Protobufs projects.
 *
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#ifndef CM_H
#define CM_H

#include "net.h"

/* EMsg values used here (SteamKit2 SteamLanguage/emsg.steamd). */
enum {
	EMSG_MULTI = 1,
	EMSG_SERVICE_METHOD_RESPONSE = 147,
	EMSG_SERVICE_METHOD_CALL = 151,
	EMSG_CLIENT_HEARTBEAT = 703,
	EMSG_CLIENT_LOGOFF = 706,
	EMSG_CLIENT_LOGON_RESPONSE = 751,
	EMSG_CLIENT_LOGGED_OFF = 757,
	EMSG_CLIENT_LICENSE_LIST = 780,
	EMSG_CLIENT_GET_DEPOT_KEY = 5438,
	EMSG_CLIENT_GET_DEPOT_KEY_RESPONSE = 5439,
	EMSG_CLIENT_LOGON = 5514,
	EMSG_CLIENT_REQUEST_FREE_LICENSE = 5572,
	EMSG_CLIENT_REQUEST_FREE_LICENSE_RESPONSE = 5573,
	EMSG_CLIENT_PICS_PRODUCT_INFO_REQUEST = 8903,
	EMSG_CLIENT_PICS_PRODUCT_INFO_RESPONSE = 8904,
	EMSG_CLIENT_PICS_ACCESS_TOKEN_REQUEST = 8905,
	EMSG_CLIENT_PICS_ACCESS_TOKEN_RESPONSE = 8906,
	EMSG_SERVICE_METHOD_CALL_NONAUTHED = 9804,
	EMSG_CLIENT_HELLO = 9805,
};

#define PROTO_MASK		0x80000000u
#define PROTOCOL_VERSION	65580

struct cm_msg {
	uint32_t	 emsg;
	uint64_t	 jobid_target;
	int32_t		 eresult;
	char		*error_message;
	struct buf	 raw;		/* whole message */
	const uint8_t	*body;
	size_t		 blen;
};

struct license {
	uint32_t	package_id;
	uint64_t	access_token;
};

struct cm {
	struct conn	 c;
	uint64_t	 steamid;
	int32_t		 session_id;
	uint32_t	 cell_id;
	int		 heartbeat;	/* seconds */
	double		 last_send;
	uint64_t	 next_job;
	int		 logged_on;
	/* queued messages from Multi */
	struct buf	*queue;
	size_t		 nqueue, qhead;
	struct license	*licenses;
	size_t		 nlicenses;
	int		 have_licenses;
};

int	cm_connect(struct cm *);
void	cm_close(struct cm *);
int	cm_send(struct cm *, uint32_t emsg, const struct buf *body,
	    uint64_t jobid_source, const char *target_job_name);
/* Next message, up to timeout seconds (-1: forever).  0 ok, -1 error/timeout. */
int	cm_recv(struct cm *, struct cm_msg *, int timeout);
void	cm_msg_free(struct cm_msg *);
/* Send and wait for the message with the given emsg (and job, if nonzero). */
int	cm_call(struct cm *, uint32_t emsg, const struct buf *body,
	    uint32_t want, struct cm_msg *);
/* Unified service method ("Service.Method#1"); authed or not. */
int	cm_service(struct cm *, const char *method, const struct buf *req,
	    struct cm_msg *resp);

/* account_name NULL: anonymous.  Returns EResult (1 = OK). */
int	cm_logon(struct cm *, const char *account_name, const char *token);
int	cm_wait_licenses(struct cm *);

const char *eresult_str(int);

#endif
