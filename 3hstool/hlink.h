
#ifndef inc_hlink_h
#define inc_hlink_h

#ifdef __cplusplus
extern "C" {
#endif

#include <netdb.h>

enum HAction
{
	HA_add_queue    = 0,
	HA_install_id   = 1,
	HA_install_url  = 2,
	HA_install_data = 3,
	HA_nothing      = 4,
	HA_launch       = 5,
	HA_sleep        = 6,
};

enum HResponse
{
	HR_accept       = 0,
	HR_busy         = 1,
	HR_untrusted    = 2,
	HR_error        = 3,
	HR_success      = 4,
	HR_notfound     = 5,
};

enum HError
{
	HE_success      = 0, /* success */
	HE_notauthed    = 1, /* you didn't call hl_auth() or hl_auth() didn't return 0 */
	HE_tryagain     = 2, /* try again, caused by the server being busy */
	HE_tidnotfound  = 3, /* you tried to interact with a title that was not installed */
	HE_exterror     = 4, /* extended error. an error message from the 3ds */
};

typedef struct hLink
{
	struct addrinfo *host;
	int isauthed;
} hLink;

/* connects a link, get an error with hl_makelink_geterror */
int hl_makelink(hLink *link, const char *addr);
/* frees memory used by link */
void hl_destroylink(hLink *link);
/* gets a human readable error string from hl_makelink */
const char *hl_makelink_geterror(int errcode);
/* gets a human readable error string */
const char *hl_geterror(int errcode);
/* authenticates with the server */
int hl_auth(hLink *link);
/* launch a title on the 3ds with a title id */
int hl_launch(hLink *link, uint64_t tid);
/* sends hshop ids to add to the queue */
int hl_addqueue(hLink *link, uint64_t *ids, size_t amount);
/* sleeps the 3ds for 5 seconds */
int hl_sleep(hLink *link);
/* wait on host for a bit because the 3ds is garbage */
void hl_waittimeout(void);

#ifdef __cplusplus
}
#endif

#endif

