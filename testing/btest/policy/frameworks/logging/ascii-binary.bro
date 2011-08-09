#
# @TEST-EXEC: bro -b %INPUT
# @TEST-EXEC: btest-diff ssh.log

module SSH;

export {
	redef enum Log::ID += { SSH };

	type Info: record {
		data: string;
		data2: string;
	} &log;
}

redef LogAscii::separator = "|";

event bro_init()
{
	Log::create_stream(SSH, [$columns=Info]);
	Log::write(SSH, [$data="abc\n\xffdef", $data2="DATA2"]);
	Log::write(SSH, [$data="abc|\xffdef", $data2="DATA2"]);
	Log::write(SSH, [$data="abc\xff|def", $data2="DATA2"]);
}

