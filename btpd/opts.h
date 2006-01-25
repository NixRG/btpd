#ifndef BTPD_OPTS_H
#define BTPD_OPTS_H

extern short btpd_daemon;
extern const char *btpd_dir;
extern uint32_t btpd_logmask;
extern int net_max_downloaders;
extern unsigned net_max_peers;
extern unsigned net_bw_limit_in;
extern unsigned net_bw_limit_out;
extern int net_port;
extern off_t cm_alloc_size;

#endif
