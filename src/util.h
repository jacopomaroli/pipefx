#ifndef _UTIL_H_
#define _UTIL_H_

#include "conf.h"
#include "fx_chain_utils.h"

unsigned power2(unsigned v);
void daemonize(void);

#ifdef __cplusplus
extern "C"
#endif
	int parse_config(char* buf, conf_t* config);

#ifdef __cplusplus
extern "C"
#endif
	void print_config(conf_t* config);

#ifdef __cplusplus
extern "C"
#endif
	void get_config(conf_t* config, char* config_file_path);

#endif // _UTIL_H_