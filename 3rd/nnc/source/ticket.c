
#include <nnc/ticket.h>
#include <string.h>
#include "./internal.h"


nnc_result nnc_read_ticket(nnc_rstream *rs, nnc_ticket *tik)
{
	result ret;
	TRY(nnc_read_sig(rs, &tik->sig));
	u8 buf[0x124];
	TRY(read_exact(rs, buf, sizeof(buf)));
	/* 0x00 */ memcpy(tik->ecc_pubkey, &buf[0x00], 0x3C);
	/* 0x3C */ tik->version = buf[0x3C];
	/* 0x3D */ tik->cacrlversion = buf[0x3D];
	/* 0x3E */ tik->signercrlversion = buf[0x3E];
	/* 0x3F */ memcpy(tik->title_key, &buf[0x3F], 0x10);
	/* 0x4F */ /* reserved */
	/* 0x50 */ tik->ticket_id = BE64P(&buf[0x50]);
	/* 0x58 */ tik->console_id = BE32P(&buf[0x58]);
	/* 0x5C */ tik->title_id = BE64P(&buf[0x5C]);
	/* 0x64 */ /* reserved */
	/* 0x66 */ tik->title_version = BE16P(&buf[0x66]);
	/* 0x68 */ /* reserved */
	/* 0x70 */ tik->license_type = buf[0x70];
	/* 0x71 */ tik->common_keyy = buf[0x71];
	/* 0x72 */ /* reserved */
	/* 0x9C */ tik->eshop_account_id = BE32P(&buf[0x9C]);
	/* 0xA0 */ /* reserved */
	/* 0xA1 */ tik->audit = buf[0xA1];
	/* 0xA2 */ /* reserved */
	/* 0xE4 */ memcpy(tik->limits, &buf[0xE4], 0x40);
	return NNC_R_OK;
}

