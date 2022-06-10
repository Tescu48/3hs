
#include <nnc/read-stream.h>
#include <nnc/ticket.h>
#include <inttypes.h>

void nnc_dumpmem(const nnc_u8 *b, nnc_u32 size);
void die(const char *fmt, ...);


int tik_main(int argc, char *argv[])
{
	if(argc != 2) die("usage: %s <tik-file>", argv[0]);
	const char *tik_file = argv[1];

	nnc_file f;
	if(nnc_file_open(&f, tik_file) != NNC_R_OK)
		die("failed to open '%s'", tik_file);

	nnc_ticket tik;
	if(nnc_read_ticket(NNC_RSP(&f), &tik) != NNC_R_OK)
		die("failed to read ticket from '%s'", tik_file);

	printf(
		"== %s ==\n"
		"  Signature Type         : %s\n"
		"  Signature Issuer       : %s\n"
		"  ECC Public Key         :\n"
	, tik_file, nnc_sigstr(tik.sig.type), tik.sig.issuer);
	nnc_dumpmem(tik.ecc_pubkey, sizeof(tik.ecc_pubkey));
	printf(
		"  Version                : %i\n"
		"  CA CRL Version         : %i\n"
		"  Signer CRL Version     : %i\n"
		"  Title Key (Encrypted)  : "
	, tik.version, tik.cacrlversion, tik.signercrlversion);
	for(int i = 0; i < 0x10; ++i) printf("%02X", tik.title_key[i]);
	puts("");
	nnc_u8 major, minor, patch;
	nnc_parse_version(tik.title_version, &major, &minor, &patch);
	printf(
		"  Ticket ID              : %016" PRIX64 "\n"
		"  Console ID             : %08" PRIX32 "\n"
		"  Title ID               : %016" PRIX64 "\n"
		"  Title Version          : %i.%i.%i (v%i)\n"
		"  License Type           : %i\n"
		"  Common KeyY            : %i\n"
		"  eShop Account ID       : %08" PRIX32 "\n"
		"  Audit                  : %i\n"
		"  Limits                 :\n"
	, tik.ticket_id, tik.console_id, tik.title_id
	, major, minor, patch, tik.title_version
	, tik.license_type, tik.common_keyy, tik.eshop_account_id, tik.audit);
	nnc_dumpmem(tik.limits, 0x40);

	NNC_RS_CALL0(f, close);
	return 0;
}

