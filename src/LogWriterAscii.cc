// See the file "COPYING" in the main distribution directory for copyright.

#include <string>
#include <cerrno>
#include <cstdio>

#include "LogWriterAscii.h"
#include "NetVar.h"

static string _GetBroTypeString(const LogField *field)
{
	switch(field->type)
	{
	case TYPE_BOOL:
		return "bool";
	case TYPE_COUNT:
		return "count";
	case TYPE_COUNTER:
		return "counter";
	case TYPE_PORT:
		return "port";
	case TYPE_INT:
		return "int";
	case TYPE_TIME:
		return "time";
	case TYPE_INTERVAL:
		return "interval";
	case TYPE_DOUBLE:
		return "double"; 
	case TYPE_SUBNET:
		return "subnet";
	case TYPE_NET:
		return "net";
	case TYPE_ADDR:
		return "addr";
	case TYPE_ENUM:
		return "enum";
	case TYPE_STRING:
		return "string";
	case TYPE_FILE:
		return "file";
	case TYPE_TABLE:
		return "table";
	case TYPE_VECTOR:
		return "vector";
	default:
		return "???";
	}
}

LogWriter* LogWriterAscii::Instantiate(LogEmissary& parent, QueueInterface<MessageEvent *>& in_queue, QueueInterface<MessageEvent *>& out_queue)	
{ 
	return new LogWriterAscii(parent, in_queue, out_queue); 
}

LogWriterAscii::LogWriterAscii(LogEmissary& parent, QueueInterface<MessageEvent *>& in_queue, QueueInterface<MessageEvent *>& out_queue)
	: LogWriter(parent, in_queue, out_queue)
	{
	file = NULL;

	output_to_stdout = BifConst::LogAscii::output_to_stdout;
	include_header = BifConst::LogAscii::include_header;

	separator_len = BifConst::LogAscii::separator->Len();
	separator = new char[separator_len];
	memcpy(separator, BifConst::LogAscii::separator->Bytes(),
	       separator_len);

	set_separator_len = BifConst::LogAscii::set_separator->Len();
	set_separator = new char[set_separator_len];
	memcpy(set_separator, BifConst::LogAscii::set_separator->Bytes(),
	       set_separator_len);

	empty_field_len = BifConst::LogAscii::empty_field->Len();
	empty_field = new char[empty_field_len];
	memcpy(empty_field, BifConst::LogAscii::empty_field->Bytes(),
	       empty_field_len);

	unset_field_len = BifConst::LogAscii::unset_field->Len();
	unset_field = new char[unset_field_len];
	memcpy(unset_field, BifConst::LogAscii::unset_field->Bytes(),
	       unset_field_len);

	header_prefix_len = BifConst::LogAscii::header_prefix->Len();
	header_prefix = new char[header_prefix_len];
	memcpy(header_prefix, BifConst::LogAscii::header_prefix->Bytes(),
	       header_prefix_len);

	}

LogWriterAscii::~LogWriterAscii()
	{
	if ( file )
		fclose(file);

	delete [] separator;
	delete [] set_separator;
	delete [] empty_field;
	delete [] unset_field;
	delete [] header_prefix;
	}

bool LogWriterAscii::DoInit(string path, int num_fields,
			    const LogField* const * fields)
	{
	if ( output_to_stdout )
		path = "/dev/stdout";

	fname = IsSpecial(path) ? path : path + ".log";

	if ( ! (file = fopen(fname.c_str(), "w")) )
		{
		Error(Fmt("cannot open %s: %s", fname.c_str(),
			  strerror(errno)));
		assert(file);
		return false;
		}

	if ( include_header )
		{
		if ( fwrite(header_prefix, header_prefix_len, 1, file) != 1 )
			goto write_error;

		for ( int i = 0; i < num_fields; i++ )
			{
			if ( i > 0 &&
			     fwrite(" ", strlen(" "), 1, file) != 1 )
				goto write_error;

			const LogField* field = fields[i];

			if ( fputs(field->name.c_str(), file) == EOF )
				goto write_error;
			}
		
		if ( fputc('\n', file) == EOF )
			goto write_error;
		
		string wString = string(header_prefix, header_prefix_len) + string("path:'") + parent.Path() + string("'\n");
		wString += string(header_prefix, header_prefix_len) + "separator:'" + string(separator, separator_len) + "'\n";
		if(fwrite(wString.c_str(), wString.length(), 1, file) != 1)
			goto write_error;

		wString = string(header_prefix, header_prefix_len);
		for ( int i = 0; i < num_fields; ++i )
			{
			const LogField* field = fields[i];
			wString += ((i > 0) ? string(" ") : string("")) + field->name + string("=") + _GetBroTypeString(field);
			}
		if(fwrite(wString.c_str(), wString.length(), 1, file) != 1)
			goto write_error;
		if ( fputc('\n', file) == EOF )
			goto write_error;
		}

	assert(file);
	return true;

write_error:
	Error(Fmt("error writing to %s: %s", fname.c_str(), strerror(errno)));
	assert(file);
	return false;
	}

bool LogWriterAscii::DoFlush()
	{
	fflush(file);
	return true;
	}

void LogWriterAscii::DoFinish()
	{
	assert(file);
	// printf("FINISH: %s\n", parent.Path().c_str());
	fclose(file);
	file = NULL;
	}

bool LogWriterAscii::DoWriteOne(ODesc* desc, LogVal* val, const LogField* field)
	{
	if ( ! val->present )
		{
		desc->AddN(unset_field, unset_field_len);
		return true;
		}

	switch ( val->type ) {

	case TYPE_BOOL:
		desc->Add(val->val.int_val ? "T" : "F");
		break;

	case TYPE_INT:
		desc->Add(val->val.int_val);
		break;

	case TYPE_COUNT:
	case TYPE_COUNTER:
	case TYPE_PORT:
		desc->Add(val->val.uint_val);
		break;

	case TYPE_SUBNET:
		desc->Add(dotted_addr_r(val->val.subnet_val.net, strbuf, LOGWRITER_MAX_BUFSZ));
		desc->Add("/");
		desc->Add(val->val.subnet_val.width);
		break;

	case TYPE_NET:
	case TYPE_ADDR:
		desc->Add(dotted_addr_r(val->val.addr_val, strbuf, LOGWRITER_MAX_BUFSZ));
		break;

	case TYPE_TIME:
		char buf[32];
		snprintf(buf, sizeof(buf), "%.6f", val->val.double_val);
		desc->Add(buf);
		break;

	case TYPE_DOUBLE:
	case TYPE_INTERVAL:
		desc->Add(val->val.double_val);
	break;

	case TYPE_ENUM:
	case TYPE_STRING:
	case TYPE_FILE:
		{
		int size = val->val.string_val->size();
		if ( size )
			desc->AddN(val->val.string_val->data(), val->val.string_val->size());
		else
			desc->AddN(empty_field, empty_field_len);
		break;
		}

	case TYPE_TABLE:
		{
		if ( ! val->val.set_val.size )
			{
			desc->AddN(empty_field, empty_field_len);
			break;
			}

		for ( int j = 0; j < val->val.set_val.size; j++ )
			{
			if ( j > 0 )
				desc->AddN(set_separator, set_separator_len);

			if ( ! DoWriteOne(desc, val->val.set_val.vals[j], field) )
				return false;
			}

		break;
		}

	case TYPE_VECTOR:
		{
		if ( ! val->val.vector_val.size )
			{
			desc->AddN(empty_field, empty_field_len);
			break;
			}

		for ( int j = 0; j < val->val.vector_val.size; j++ )
			{
			if ( j > 0 )
				desc->AddN(set_separator, set_separator_len);

			if ( ! DoWriteOne(desc, val->val.vector_val.vals[j], field) )
				return false;
			}

		break;
		}

	default:
		Error(Fmt("unsupported field format %d for %s", val->type,
			  field->name.c_str()));
		return false;
	}

	return true;
	}

bool LogWriterAscii::DoWrite(int num_fields, const LogField* const * fields,
			     LogVal** vals)
	{
	ODesc desc(DESC_READABLE);
	desc.SetEscape(separator, separator_len);

	for ( int i = 0; i < num_fields; i++ )
		{
		if ( i > 0 )
			desc.AddRaw(separator, separator_len);

		if ( ! DoWriteOne(&desc, vals[i], fields[i]) )
			return false;
		}

	desc.AddRaw("\n", 1);

	if ( fwrite(desc.Bytes(), desc.Len(), 1, file) != 1 )
		{
		Error(Fmt("error writing to %s: %s", fname.c_str(), strerror(errno)));
		return false;
		}

	if ( !IsBuf() )
		fflush(file);

	return true;
	}

bool LogWriterAscii::DoRotate(string rotated_path, double open,
			      double close, bool terminating)
	{
	
	if ( IsSpecial(parent.Path()) )
		// Don't rotate special files.
		return true;

	// printf("Rotating: %s\n", parent.Path().c_str());
	if(file)
		fclose(file);

	string nname = rotated_path + ".log";
	rename(fname.c_str(), nname.c_str());

	if ( ! FinishedRotation(nname, fname, open, close, terminating) )
		Error(Fmt("error rotating %s to %s", fname.c_str(), nname.c_str()));

	return DoInit(parent.Path(), parent.NumFields(), parent.Fields());
	}

bool LogWriterAscii::DoSetBuf(bool enabled)
	{
	// Nothing to do.
	return true;
	}

// Register the ASCII logger so that Bro can use it.
static LogWriterRegistrar __register_logger(BifEnum::Log::WRITER_ASCII, "Ascii", NULL, LogWriterAscii::Instantiate);

