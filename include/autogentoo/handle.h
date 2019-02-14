#ifndef __AUTOGENTOO_HANDLE_H__
#define __AUTOGENTOO_HANDLE_H__

#include "request.h"

#define HANDLE_SET_STATUS(ret)({ \
dynamic_binary_add(res->content, 'i', &(ret).code); \
dynamic_binary_add(res->content, 's', (ret).message); \
res->code = ret; \
})

#define HANDLE_RETURN(ret) ({ \
HANDLE_SET_STATUS((ret)); \
return; \
})

#define HANDLE_CHECK_STRUCTURES(...) (\
{ \
	int check_types[] = __VA_ARGS__; \
	int __check_types_n = sizeof (check_types) / sizeof (int); \
	if (__check_types_n != request->struct_c) \
		HANDLE_RETURN(BAD_REQUEST); \
	if (prv_check_data_structs (request, check_types, __check_types_n) == -1) \
		HANDLE_RETURN(BAD_REQUEST); \
});

/**
 * Holds all of the valid requests
 */
extern RequestLink requests[];

/**
 * Parse the request and write the arguemnts to args
 * @param type resolved type from request_handle
 * @return a pointer to function that should be called
 */
FunctionHandler resolve_call(request_t type);


/**
 * HTTP Requests
 * Just normal http
 * We don't do POST around here
 */
void GET(Connection* conn, HttpRequest* req);
void HEAD(Connection* conn, HttpRequest* req);


/**
 * PROT_AUTOGENTOO
 */
void HOST_NEW(Response* res, Request* request);
void HOST_EDIT(Response* res, Request* request);
void HOST_DEL(Response* res, Request* request);
void HOST_EMERGE(Response* res, Request* request);

void SRV_MNTCHROOT(Response* res, Request* request);
void SRV_INFO(Response* res, Request* request);

#endif