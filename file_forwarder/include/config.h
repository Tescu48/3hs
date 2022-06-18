
#pragma once
#include <3ds.h>

#define STRNCMP(var,val) (strcmp(var, val) != 0)
#define STRCMP(var,val) (strcmp(var, val) == 0)
#define KVPARSER_DEFAULT_SEEK 0
#define KVBUF 128


typedef struct {
	char value[KVBUF];
	char key[KVBUF];
	int status;
} KeyValue;

typedef struct {
	char *data;
	char sepk;
	char sepv;
	int loc;
} KVParser;


enum KVWhere {
	WHERE_VALUE = 1,
	WHERE_KEY = 0,
};

enum KVStatus {
	KVS_SUCCESS = 1 << 0,
	KVS_FAILED = 1 << 1,
	KVS_HAD_NL = 1 << 2,
};


void set_parser_seperator(KVParser *parser, char sepv, char sepk);
void set_parser_data(KVParser *parser, char *data);
void set_parser_seek(KVParser *parser, int num);
KeyValue get_next_token(KVParser *parser);
