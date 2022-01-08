#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "json.h"

#define DATA_MAX_PATH				4096

#define DATA_INPUT_FILE_NAME		"data.json"
#define DATA_OUTPUT_TLV_FILE_NAME	"data.tlv"
#define DATA_OUTPUT_DICT_FILE_NAME	"data.dict"

#define LOG_MAX_LEN					(DATA_MAX_PATH * 2)

typedef struct record_tlv_s
{
	int			id;
	json_type	type;

	char		boolean;
	int			integer;
	double		dbl;
	char		*string;

	struct record_tlv_s		*next;
} record_tlv_t;

typedef enum data_type_e
{
	DATA_TYPE_INAVLID = 0,

	DATA_TYPE_SIMPLE,
	DATA_TYPE_DICT
} data_type_t;

static void _printf(const char *file, int line, const char *function, const char *fmt, va_list ap)
{
	char	msg[LOG_MAX_LEN];
	char	out[LOG_MAX_LEN - 256];

	vsnprintf(out, sizeof(out), fmt, ap);
	snprintf(msg, sizeof(msg), "%s:%u:%s %s\n", file, line, function, out);

	printf("%s", (const char *) msg);
}

static void _plog(const char *file, int line, const char *function, const char *fmt, ...)
{
	va_list va;

	if (fmt)
	{
		va_start(va, fmt);
		_printf(file, line, function, fmt, va);
		va_end(va);
	}
}

#define PLOG(__fmt, ...)	_plog(__FILE__, __LINE__, __FUNCTION__, __fmt, ##__VA_ARGS__)

static void _record_free(record_tlv_t **record);

static record_tlv_t *_record_init(record_tlv_t *record, int id, json_type type, char boolean, int integer, double dbl, const char *str, size_t strsize)
{
	memset(record, 0, sizeof(record_tlv_t));

	record->id = id;
	record->type = type;
	record->boolean = boolean;
	record->integer = integer;
	record->dbl = dbl;

	if (strsize)
	{
		PLOG("alloc record with strsize %d", strsize);
		record->string = malloc(strsize + 1);
		if (!record->string)
		{
			PLOG("failed on alloc string size %d", strsize + 1);
			_record_free(&record);
			return NULL;
		}
	}

	if (str && record->string)
	{
		memcpy(record->string, str, strsize);
		record->string[strsize] = '\0';
		PLOG("copy string str '%s'", record->string);
	}

	return record;
}

static record_tlv_t *_record_alloc(int id, json_type type, char boolean, int integer, double dbl, const char *str, size_t strsize)
{
	record_tlv_t *record = malloc(sizeof(record_tlv_t));

	if (!record)
	{
		PLOG("failed on alloc record_tlv_t");
		return NULL;
	}

	return _record_init(record, id, type, boolean, integer, dbl, str, strsize);
}

static void _record_free(record_tlv_t **record)
{
	record_tlv_t *_record = *record;

	if (!_record)
		return;

	if (_record->string)
		free(_record->string);

	free(_record);
	*record = NULL;
}

static void _records_free(record_tlv_t **record)
{
	record_tlv_t	*_record = *record;
	record_tlv_t	*next = NULL;

	if (!_record)
		return;

	next = _record;
	do
	{
		_record = next->next;
		_record_free(&next);
		next = _record;
	} while (next);

	*record = NULL;
}

static int _record_save(FILE *output, record_tlv_t *record)
{
	int		rc;
	void	*value;
	size_t	length;

	PLOG("save id %d type %d", record->id, record->type);

	if (!fwrite(&record->type, sizeof(record->type), 1, output))
	{
		PLOG("failed on fwrite type");
		return -1;
	}

	switch (record->type)
	{
		case json_integer:
			length = sizeof(record->integer);
			break;

		case json_double:
			length = sizeof(record->dbl);
			break;

		case json_string:
			length = strlen(record->string);
			break;

		case json_boolean:
			length = sizeof(record->boolean);
			break;

		default:
			length = 0;
			break;
	}

	switch (record->type)
	{
		case json_integer:
			value = &record->integer;
			break;

		case json_double:
			value = &record->dbl;
			break;

		case json_string:
			value = record->string;
			break;

		case json_boolean:
			value = &record->boolean;
			break;

		default:
			value = NULL;
			break;
	}

	if (value && length)
	{
		PLOG("save length %d", length);

		rc = fwrite(&length, sizeof(length), 1, output);
		if (!rc)
		{
			PLOG("failed on fwrite length");
			return -1;
		}

		if (record->type == json_string)
			PLOG("save string value '%s'", record->string);
		else
			PLOG("save value");

		rc = fwrite(value, length, 1, output);
		if (!rc)
		{
			PLOG("failed on fwrite value");
			return -1;
		}
	}

	return 0;
}

static int _records_save(record_tlv_t *record, data_type_t data_type, const char *filename)
{
	record_tlv_t	*next = record;
	FILE			*output;
	int				rc;

	if (!next)
		return 0;

	output = fopen(filename, "wb");
	if (!output)
	{
		PLOG("failed on fopen file '%s'", filename);
		return -1;
	}

	rc = 0;
	while (next)
	{
		if (data_type == DATA_TYPE_DICT)
		{
			record_tlv_t	dict_record;

			if (!_record_init(&dict_record, next->id, json_integer, 0, next->id, 0.0, NULL, 0))
			{
				PLOG("failed on _record_init dict id");
				rc = -1;
				break;
			}

			if (_record_save(output, &dict_record))
			{
				PLOG("failed on _record_save dict id");
				rc = -1;
				break;
			}
		}

		if (_record_save(output, next))
		{
			PLOG("failed on _record_save");
			rc = -1;
			break;
		}

		next = next->next;
	}

	fclose(output);

	return rc;
}

static int _read_line(FILE *file, char *line, size_t *linesize)
{
	size_t	size, ret, total;
	char	buf;

	total = 0;
	do
	{
		size = 1;
		ret = fread(&buf, size, 1, file);

		if (ret != size)
		{
			if (feof(file))
				return 1;

			PLOG("failed on fread");
			return -1;
		}

		if (total + 1 >= *linesize)
		{
			PLOG("error size");
			return -1;
		}

		//PLOG("read '%c'", buf);
		line[total] = buf;
		if (buf != '\n')
			total++;

	} while (buf != '\n');

	*linesize = total;
	line[total] = '\0';

	return 0;
}

static record_tlv_t *_process_object(json_value *value, int id)
{
	int				i, count;
	record_tlv_t	*next, *prev, *head;
	json_value		*item;
	int				proc;

	next = NULL;
	prev = NULL;
	head = NULL;

	count = value->u.object.length;
	for (i = 0; i < count; i++)
	{
		PLOG("name '%s'", value->u.object.values[i].name);

		proc = 1;
		item = value->u.object.values[i].value;

		PLOG("type %d", item->type);

		switch (item->type)
		{
			case json_integer:
				PLOG("int: %d", item->u.integer);
				next = _record_alloc(id, item->type, 0, item->u.integer, 0.0, NULL, 0);
				break;

			case json_double:
				PLOG("double: %f", item->u.dbl);
				next = _record_alloc(id, item->type, 0, 0, item->u.dbl, NULL, 0);
				break;

			case json_string:
				PLOG("string: %s", item->u.string.ptr);
				next = _record_alloc(id, item->type, 0, 0, 0.0, item->u.string.ptr, item->u.string.length);
				break;

			case json_boolean:
				PLOG("bool: %d", item->u.boolean);
				next = _record_alloc(id, item->type, item->u.boolean, 0, 0.0, NULL, 0);
				break;

			default:
				proc = 0;
				break;
		}

		if (!head)
			head = next;

		if (prev)
			prev->next = next;

		if (proc && !next)
		{
			PLOG("failed on _record_alloc");
			_records_free(&head);
			return NULL;
		}

		id++;
		prev = next;
	}

	return head;
}

static record_tlv_t *_parse_line(const char *string, int depth)
{
	json_value		*value;
	record_tlv_t	*record;
	int				id = 1;

	value = json_parse(string, strlen(string));
	if (!value)
	{
		PLOG("failed on json_parse");
		return NULL;
	}

	PLOG("process json");
	record = _process_object(value, id);

	json_value_free(value);

	return record;
}

int main(int argc, char *argv[])
{
	int				ret;
	char			data[DATA_MAX_PATH];
	size_t			i, size = DATA_MAX_PATH;
	FILE			*input = NULL;
	record_tlv_t	*next = NULL;
	record_tlv_t	*prev = NULL;
	record_tlv_t	*head = NULL;

	input = fopen(DATA_INPUT_FILE_NAME, "rb");
	if (!input)
	{
		PLOG("no file '%s'", DATA_INPUT_FILE_NAME);
		return -1;
	}

	PLOG("start parse");

	i = 0;
	while ((ret = _read_line(input, data, &size)) == 0)
	{
		PLOG("json line number %d (line size %d): '%s'", i, size, data);

		if (size)
		{
			next = _parse_line(data, 0);
			if (!next)
			{
				PLOG("failed on _parse_line");
				break;
			}

			if (!head)
				head = next;

			if (prev)
				prev->next = next;

			while (next->next)
				next = next->next;

			prev = next;
		}

		size = DATA_MAX_PATH;
		i++;
	}

	if (ret == 0)
		PLOG("something wrong");
	else if (ret < 0)
		PLOG("failed on read line");
	else if (ret > 0)
		PLOG("end of file");

	fclose(input);

	if (ret <= 0)
	{
		_records_free(&head);
		return -1;
	}

	if (_records_save(head, DATA_TYPE_SIMPLE, DATA_OUTPUT_TLV_FILE_NAME))
		PLOG("failed on _records_save");

	if (_records_save(head, DATA_TYPE_DICT, DATA_OUTPUT_DICT_FILE_NAME))
		PLOG("failed on _records_save");

	_records_free(&head);

	return 0;
}
