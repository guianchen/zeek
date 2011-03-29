## This script extracts and logs variables from the requested URI

@load http/utils

module HTTP;

export {
	redef record State += {
		# TODO: This will change to be initialized to an empty vector when possible.
		uri_vars:    vector of string &optional &log;
	};
}

event http_request(c: connection, method: string, original_URI: string,
                   unescaped_URI: string, version: string) &priority=2
	{
	c$http$uri_vars = extract_keys(original_URI, /&/);
	}