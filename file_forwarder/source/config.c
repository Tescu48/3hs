
#include "config.h"


KeyValue get_next_token(KVParser *parser)
{

	char *data = parser->data + parser->loc;
	KeyValue ret;

	ret.status = KVS_SUCCESS;
	int where = WHERE_KEY;

	for(int i = 0, j = 0, kvd = 0; aptMainLoop(); ++i)
	{
		// Break loop
		if(data[i] == parser->sepk)
		{
			ret.value[j] = '\0';
			parser->loc += i + 1;

			if(kvd == 0)
			{
				ret.status = KVS_FAILED;
				break;
			}

			break;
		}
		else if(data[i] == parser->sepv)
		{
			ret.key[j] = '\0';

			if(kvd == 1)
			{
				ret.status = KVS_FAILED;
				break;
			}

			++where;
			++kvd;
			j = 0;
		}
		else
		{
			if(where == WHERE_KEY || i > KVBUF)
			{
				ret.key[j] = data[i];
				++j;
			}
			else if(where == WHERE_VALUE || i > KVBUF)
			{
				ret.value[j] = data[i];
				++j;
			}
			else
			{
				ret.status = KVS_FAILED;
				break;
			}
		}
	}

	return ret;
}

void set_parser_data(KVParser *parser, char *data)
{
	parser->data = data;
}

void set_parser_seperator(KVParser *parser, char sepv, char sepk)
{
	parser->sepv = sepv;
	parser->sepk = sepk;
}

void set_parser_seek(KVParser *parser, int num)
{
	parser->loc = num;
}
