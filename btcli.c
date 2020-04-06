#include <stdarg.h>

#ifndef BTCLI_H
#define BTCLI_H

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "btpd_if.h"
#include "metainfo.h"
#include "subr.h"
#include "benc.h"
#include "iobuf.h"
#include "queue.h"

#include "utils.h"

extern const char *btpd_dir;
extern struct ipc *ipc;

__attribute__((noreturn))
void diemsg(const char *fmt, ...);
void btpd_connect(void);
enum ipc_err handle_ipc_res(enum ipc_err err, const char *cmd,
    const char *target);
char tstate_char(enum ipc_tstate ts);
int torrent_spec(char *arg, struct ipc_torrent *tp);

void print_rate(long long rate);
void print_size(long long size);
void print_ratio(long long part, long long whole);
void print_percent(long long part, long long whole);

void usage_add(void);
void cmd_add(int argc, char **argv);
void usage_del(void);
void cmd_del(int argc, char **argv);
void usage_list(void);
void cmd_list(int argc, char **argv);
void usage_stat(void);
void cmd_stat(int argc, char **argv);
void usage_kill(void);
void cmd_kill(int argc, char **argv);
void usage_rate(void);
void cmd_rate(int argc, char **argv);
void usage_start(void);
void cmd_start(int argc, char **argv);
void usage_stop(void);
void cmd_stop(int argc, char **argv);

#endif

const char *btpd_dir;
struct ipc *ipc;

void
diemsg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}

void
btpd_connect(void)
{
    if ((errno = ipc_open(btpd_dir, &ipc)) != 0)
        diemsg("cannot open connection to btpd in %s (%s).\n", btpd_dir,
            strerror(errno));
}

enum ipc_err
handle_ipc_res(enum ipc_err code, const char *cmd, const char *target)
{
    switch (code) {
    case IPC_OK:
        break;
    case IPC_COMMERR:
        diemsg("error in communication with btpd.\n");
    default:
        fprintf(stderr, "btcli %s '%s': %s.\n", cmd, target,
            ipc_strerror(code));
    }
    return code;
}

void
print_percent(long long part, long long whole)
{
    printf("%5.1f%% ", floor(1000.0 * part / whole) / 10);
}

void
print_rate(long long rate)
{
    if (rate >= 999.995 * (1 << 10))
        printf("%6.2fMB/s ", (double)rate / (1 << 20));
    else
        printf("%6.2fkB/s ", (double)rate / (1 << 10));
}

void
print_size(long long size)
{
    if (size >= 999.995 * (1 << 20))
        printf("%6.2fG ", (double)size / (1 << 30));
    else
        printf("%6.2fM ", (double)size / (1 << 20));
}
void
print_ratio(long long part, long long whole)
{
    printf("%7.2f ", (double)part / whole);
}

char
tstate_char(enum ipc_tstate ts)
{
    switch (ts) {
    case IPC_TSTATE_INACTIVE:
        return 'I';
    case IPC_TSTATE_START:
        return '+';
    case IPC_TSTATE_STOP:
        return '-';
    case IPC_TSTATE_LEECH:
        return 'L';
    case IPC_TSTATE_SEED:
        return 'S';
    }
    diemsg("unrecognized torrent state.\n");
}

int
torrent_spec(char *arg, struct ipc_torrent *tp)
{
    char *p;
    tp->u.num = strtoul(arg, &p, 10);
    if (*p == '\0') {
        tp->by_hash = 0;
        return 1;
    }
    if ((p = mi_load(arg, NULL)) == NULL) {
        fprintf(stderr, "btcli: bad torrent '%s' (%s).\n", arg,
            strerror(errno));
        return 0;
    }
    tp->by_hash = 1;
    mi_info_hash(p, tp->u.hash);
    free(p);
    return 1;
}

static struct {
    const char *name;
    void (*fun)(int, char **);
    void (*help)(void);
} cmd_table[] = {
    { "add", cmd_add, usage_add },
    { "del", cmd_del, usage_del },
    { "kill", cmd_kill, usage_kill },
    { "list", cmd_list, usage_list },
    { "rate", cmd_rate, usage_rate },
    { "start", cmd_start, usage_start },
    { "stop", cmd_stop, usage_stop },
    { "stat", cmd_stat, usage_stat }
};

static void
usage(void)
{
    printf(
        "btcli is the btpd command line interface.\n"
        "\n"
        "Usage: btcli [main options] command [command options]\n"
        "\n"
        "Main options:\n"
        "-d dir\n"
        "\tThe btpd directory.\n"
        "\n"
        "--help [command]\n"
        "\tShow this text or help for the specified command.\n"
        "\n"
        "Commands:\n"
        "add\t- Add torrents to btpd.\n"
        "del\t- Remove torrents from btpd.\n"
        "kill\t- Shut down btpd.\n"
        "list\t- List torrents.\n"
        "rate\t- Set up/download rate limits.\n"
        "start\t- Activate torrents.\n"
        "stat\t- Display stats for active torrents.\n"
        "stop\t- Deactivate torrents.\n"
        "\n"
        "Note:\n"
        "Torrents can be specified either with its number or its file.\n"
        "\n"
        );
    exit(1);
}

static struct option base_opts [] = {
    { "help", no_argument, NULL, 'H' },
    {NULL, 0, NULL, 0}
};

int
main(int argc, char **argv)
{
    int ch, help = 0;

    if (argc < 2)
        usage();

    while ((ch = getopt_long(argc, argv, "+d:", base_opts, NULL)) != -1) {
        switch (ch) {
        case 'd':
            btpd_dir = optarg;
            break;
        case 'H':
            help = 1;
            break;
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc == 0)
        usage();

    if (btpd_dir == NULL)
        if ((btpd_dir = find_btpd_dir()) == NULL)
            diemsg("cannot find the btpd directory.\n");

    optind = 0;
    int found = 0;
    for (int i = 0; !found && i < ARRAY_COUNT(cmd_table); i++) {
        if (strcmp(argv[0], cmd_table[i].name) == 0) {
            found = 1;
            if (help)
                cmd_table[i].help();
            else
                cmd_table[i].fun(argc, argv);
        }
    }

    if (!found)
        usage();

    return 0;
}

/* add section */
void
usage_add(void)
{
    printf(
        "Add torrents to btpd.\n"
        "\n"
        "Usage: add [-n name] [-T] [-N] -d dir file(s)\n"
        "\n"
        "Arguments:\n"
        "file\n"
        "\tThe torrent file to add.\n"
        "\n"
        "Options:\n"
        "-d dir\n"
        "\tUse the dir for content.\n"
        "\n"
        "-n name\n"
        "\tSet the name displayed for this torrent.\n"
        "\n"
        "-l label\n"
        "\tSet the label to associate with torrent.\n"
        "\n"
        "--nostart, -N\n"
        "\tDon't activate the torrent after adding it.\n"
        "\n"
        "--topdir, -T\n"
        "\tAppend the torrent top directory (if any) to the content path.\n"
        "\n"
        );
    exit(1);
}

static struct option add_opts [] = {
    { "help", no_argument, NULL, 'H' },
    { "nostart", no_argument, NULL, 'N'},
    { "topdir", no_argument, NULL, 'T'},
    {NULL, 0, NULL, 0}
};

void
cmd_add(int argc, char **argv)
{
    int ch, topdir = 0, start = 1, nfile, nloaded = 0;
    size_t dirlen = 0, labellen = 0;
    char *dir = NULL, *name = NULL, *glabel = NULL, *label;

    while ((ch = getopt_long(argc, argv, "NTd:l:n:", add_opts, NULL)) != -1) {
        switch (ch) {
        case 'N':
            start = 0;
            break;
        case 'T':
            topdir = 1;
            break;
        case 'd':
            dir = optarg;
            if ((dirlen = strlen(dir)) == 0)
                diemsg("bad option value for -d.\n");
            break;
        case 'l':
            glabel = optarg;
            if ((labellen = strlen(dir)) == 0)
                diemsg("bad option value for -l.\n");
            break;
        case 'n':
            name = optarg;
            break;
        default:
            usage_add();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc < 1 || dir == NULL)
        usage_add();

    btpd_connect();
    char *mi;
    size_t mi_size;
    enum ipc_err code;
    char dpath[PATH_MAX];
    struct iobuf iob;

    for (nfile = 0; nfile < argc; nfile++) {
       if ((mi = mi_load(argv[nfile], &mi_size)) == NULL) {
           fprintf(stderr, "error loading '%s' (%s).\n", argv[nfile], strerror(errno));
           continue;
       }
       iob = iobuf_init(PATH_MAX);
       iobuf_write(&iob, dir, dirlen);
       if (topdir && !mi_simple(mi)) {
           size_t tdlen;
           const char *td =
               benc_dget_mem(benc_dget_dct(mi, "info"), "name", &tdlen);
           iobuf_swrite(&iob, "/");
           iobuf_write(&iob, td, tdlen);
       }
       iobuf_swrite(&iob, "\0");
       if ((errno = make_abs_path(iob.buf, dpath)) != 0) {
           fprintf(stderr, "make_abs_path '%s' failed (%s).\n", dpath, strerror(errno));
           iobuf_free(&iob);
           continue;
       }
       if(NULL == glabel)
          label = benc_dget_str(mi, "announce", NULL);
       else
          label = glabel;
       code = btpd_add(ipc, mi, mi_size, dpath, name, label);
       if ((code == IPC_OK) && start) {
           struct ipc_torrent tspec;
           tspec.by_hash = 1;
           mi_info_hash(mi, tspec.u.hash);
           code = btpd_start(ipc, &tspec);
       }
       if (code != IPC_OK) {
           fprintf(stderr, "command failed for '%s' (%s).\n", argv[nfile], ipc_strerror(code));
       } else {
           nloaded++;
       }
       iobuf_free(&iob);
    }

    if (nloaded != nfile) {
       diemsg("error loaded %d of %d files.\n", nloaded, nfile);
    }
}

/* del section */
void
usage_del(void)
{
    printf(
        "Remove torrents from btpd.\n"
        "\n"
        "Usage: del torrent ...\n"
        "\n"
        "Arguments:\n"
        "torrent ...\n"
        "\tThe torrents to remove.\n"
        "\n");
    exit(1);
}

static struct option del_opts [] = {
    { "help", no_argument, NULL, 'H' },
    {NULL, 0, NULL, 0}
};

void
cmd_del(int argc, char **argv)
{
    int ch;
    struct ipc_torrent t;

    while ((ch = getopt_long(argc, argv, "", del_opts, NULL)) != -1)
        usage_del();
    argc -= optind;
    argv += optind;

    if (argc < 1)
        usage_del();

    btpd_connect();
    for (int i = 0; i < argc; i++)
        if (torrent_spec(argv[i], &t))
            handle_ipc_res(btpd_del(ipc, &t), "del", argv[i]);
}

/* kill section */
void
usage_kill(void)
{
    printf(
        "Shutdown btpd.\n"
        "\n"
        "Usage: kill\n"
        "\n"
        );
    exit(1);
}

void
cmd_kill(int argc, char **argv)
{
    enum ipc_err code;

    if (argc > 1)
        usage_kill();

    btpd_connect();
    if ((code = btpd_die(ipc)) != 0)
        diemsg("command failed (%s).\n", ipc_strerror(code));
}

/* list section */
void
usage_list(void)
{
    printf(
        "List torrents.\n"
        "\n"
        "Usage: list [-a] [-i] [-f <format>]\n"
        "       list torrent ...\n"
        "\n"
        "Arguments:\n"
        "torrent ...\n"
        "\tThe torrents to list. Running 'btcli list' without any arguments\n"
        "\tor options is equivalent to running 'btcli list -ai'.\n"
        "\n"
        "Options:\n"
        "-a\n"
        "\tList active torrents.\n"
        "\n"
        "-i\n"
        "\tList inactive torrents.\n"
        "\n"
        );
    exit(1);
}

struct item {
    unsigned num, peers;
    char *name, *dir, *label;
    char hash[SHAHEXSIZE];
    char st;
    long long cgot, csize, totup, downloaded, uploaded, rate_up, rate_down;
    uint32_t torrent_pieces, pieces_have, pieces_seen;
    BTPDQ_ENTRY(item) entry;
};

struct items {
    int count;
    char **argv;
    int ntps;
    struct ipc_torrent *tps;
    BTPDQ_HEAD(item_tq, item) hd;
};

void
itm_insert(struct items *itms, struct item *itm)
{
    struct item *p;
    BTPDQ_FOREACH(p, &itms->hd, entry)
        if (strcmp(itm->name, p->name) < 0)
            break;
    if (p != NULL)
        BTPDQ_INSERT_BEFORE(p, itm, entry);
    else
        BTPDQ_INSERT_TAIL(&itms->hd, itm, entry);
}

static void
list_cb(int obji, enum ipc_err objerr, struct ipc_get_res *res, void *arg)
{
    struct items *itms = arg;
    struct item *itm = calloc(1, sizeof(*itm));
    if (objerr != IPC_OK)
        diemsg("list failed for '%s' (%s).\n", itms->argv[obji],
            ipc_strerror(objerr));
    itms->count++;
    itm->num   = (unsigned)res[IPC_TVAL_NUM].v.num;
    itm->peers = (unsigned)res[IPC_TVAL_PCOUNT].v.num;
    itm->st = tstate_char(res[IPC_TVAL_STATE].v.num);
    if (res[IPC_TVAL_NAME].type == IPC_TYPE_ERR)
        asprintf(&itm->name, "%s", ipc_strerror(res[IPC_TVAL_NAME].v.num));
    else
        asprintf(&itm->name, "%.*s", (int)res[IPC_TVAL_NAME].v.str.l,
            res[IPC_TVAL_NAME].v.str.p);
    if (res[IPC_TVAL_DIR].type == IPC_TYPE_ERR)
        asprintf(&itm->dir, "%s", ipc_strerror(res[IPC_TVAL_DIR].v.num));
    else
        asprintf(&itm->dir, "%.*s", (int)res[IPC_TVAL_DIR].v.str.l,
            res[IPC_TVAL_DIR].v.str.p);
    if (res[IPC_TVAL_LABEL].type == IPC_TYPE_ERR)
        asprintf(&itm->label, "%s", ipc_strerror(res[IPC_TVAL_LABEL].v.num));
    else
        asprintf(&itm->label, "%.*s", (int)res[IPC_TVAL_LABEL].v.str.l,
            res[IPC_TVAL_LABEL].v.str.p);
    bin2hex(res[IPC_TVAL_IHASH].v.str.p, itm->hash, 20);
    itm->cgot           = res[IPC_TVAL_CGOT].v.num;
    itm->csize          = res[IPC_TVAL_CSIZE].v.num;
    itm->totup          = res[IPC_TVAL_TOTUP].v.num;
    itm->downloaded     = res[IPC_TVAL_SESSDWN].v.num;
    itm->uploaded       = res[IPC_TVAL_SESSUP].v.num;
    itm->rate_up        = res[IPC_TVAL_RATEUP].v.num;
    itm->rate_down      = res[IPC_TVAL_RATEDWN].v.num;
    itm->torrent_pieces = (uint32_t)res[IPC_TVAL_PCCOUNT].v.num;
    itm->pieces_seen    = (uint32_t)res[IPC_TVAL_PCSEEN].v.num;
    itm->pieces_have    = (uint32_t)res[IPC_TVAL_PCGOT].v.num;

    itm_insert(itms, itm);
}

void
print_items(struct items* itms, char *format)
{
    struct item *p;
    char *it;
    BTPDQ_FOREACH(p, &itms->hd, entry) {
        if(format) {
            for (it = format; *it; ++it) {
                switch (*it) {
                    case '%':
                        ++it;
                        switch (*it) {
                            case '%': putchar('%');                      break;
                            case '#': printf("%u",   p->num);            break;
                            case '^': printf("%lld", p->rate_up);        break;

                            case 'A': printf("%u",   p->pieces_seen);    break;
                            case 'D': printf("%lld", p->downloaded);     break;
                            case 'H': printf("%u",   p->pieces_have);    break;
                            case 'P': printf("%u",   p->peers);          break;
                            case 'S': printf("%lld", p->csize);          break;
                            case 'U': printf("%lld", p->uploaded);       break;
                            case 'T': printf("%u",   p->torrent_pieces); break;

                            case 'd': printf("%s",   p->dir);            break;
                            case 'g': printf("%lld", p->cgot);           break;
                            case 'h': printf("%s",   p->hash);           break;
                            case 'l': printf("%s",   p->label);          break;
                            case 'n': printf("%s",   p->name);           break;
                            case 'p': print_percent(p->cgot, p->csize);  break;
                            case 'r': print_ratio(p->totup, p->csize);   break;
                            case 's': print_size(p->csize);              break;
                            case 't': printf("%c",   p->st);             break;
                            case 'u': printf("%lld", p->totup);          break;
                            case 'v': printf("%lld", p->rate_down);      break;

                            case '\0': continue;
                        }
                        break;
                    case '\\':
                        ++it;
                        switch (*it) {
                            case 'n':  putchar('\n'); break;
                            case 't':  putchar('\t'); break;
                            case '\0': continue;
                        }
                        break;
                    default: putchar(*it); break;
                }
            }
        } else {
            printf("%-40.40s %4u %c. ", p->name, p->num, p->st);
            print_percent(p->cgot, p->csize);
            print_size(p->csize);
            print_ratio(p->totup, p->csize);
            printf("\n");
        }
    }
}

static struct option list_opts [] = {
    { "format", required_argument, NULL, 'f' },
    { "help", no_argument, NULL, 'H' },
    {NULL, 0, NULL, 0}
};

void
cmd_list(int argc, char **argv)
{
    int ch, inactive = 0, active = 0;
    char *format = NULL;
    enum ipc_err code;
    enum ipc_twc twc;
    enum ipc_tval keys[] = { IPC_TVAL_NUM,    IPC_TVAL_STATE,   IPC_TVAL_NAME,
           IPC_TVAL_TOTUP,   IPC_TVAL_CSIZE,  IPC_TVAL_CGOT,    IPC_TVAL_PCOUNT,
           IPC_TVAL_PCCOUNT, IPC_TVAL_PCSEEN, IPC_TVAL_PCGOT,   IPC_TVAL_SESSUP,
           IPC_TVAL_SESSDWN, IPC_TVAL_RATEUP, IPC_TVAL_RATEDWN, IPC_TVAL_IHASH,
           IPC_TVAL_DIR, IPC_TVAL_LABEL };
    size_t nkeys = ARRAY_COUNT(keys);
    struct items itms;
    while ((ch = getopt_long(argc, argv, "aif:", list_opts, NULL)) != -1) {
        switch (ch) {
        case 'a':
            active = 1;
            break;
        case 'f':
            format = optarg;
            break;
        case 'i':
            inactive = 1;
            break;
        default:
            usage_list();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc > 0) {
        if (inactive || active)
            usage_list();
        itms.tps = malloc(argc * sizeof(*itms.tps));
        for (itms.ntps = 0; itms.ntps < argc; itms.ntps++) {
            if (!torrent_spec(argv[itms.ntps], &itms.tps[itms.ntps]))
                exit(1);

        }
    } else {
        itms.ntps = 0;
        itms.tps = NULL;
    }
    if (inactive == active)
        twc = IPC_TWC_ALL;
    else if (inactive)
        twc = IPC_TWC_INACTIVE;
    else
        twc = IPC_TWC_ACTIVE;

    btpd_connect();
    itms.count = 0;
    itms.argv = argv;
    BTPDQ_INIT(&itms.hd);
    if (itms.tps == NULL)
        code = btpd_tget_wc(ipc, twc, keys, nkeys, list_cb, &itms);
    else
        code = btpd_tget(ipc, itms.tps, itms.ntps, keys, nkeys, list_cb, &itms);
    if (code != IPC_OK)
        diemsg("command failed (%s).\n", ipc_strerror(code));
    if (format == NULL)
        printf("%-40.40s  NUM ST   HAVE    SIZE   RATIO\n", "NAME");
    print_items(&itms, format);
}

/* rate section */
void
usage_rate(void)
{
    printf(
        "Set upload and download rate.\n"
        "\n"
        "Usage: rate <up> <down>\n"
        "\n"
        "Arguments:\n"
        "<up> <down>\n"
        "\tThe up/down rate in KB/s\n"
        "\n"
        );
    exit(1);
}

static struct option rate_opts [] = {
    { "help", no_argument, NULL, 'H' },
    {NULL, 0, NULL, 0}
};

static unsigned
parse_rate(char *rate)
{
    unsigned out;
    char *end;

    out = strtol(rate, &end, 10);
    if (end == rate)
        usage_rate();

    if ((end[0] != '\0') && (end[1] != '\0'))
        usage_rate();

    switch(end[0]) {
        case 'g':
        case 'G':
            out <<= 30;
            break;
        case 'm':
        case 'M':
            out <<= 20;
            break;
        case '\0': /* default is 'k' */
        case 'k':
        case 'K':
            out <<= 10;
            break;
        case 'b':
        case 'B':
            break;
        default:
            usage_rate();
    }
    return out;
}

void
cmd_rate(int argc, char **argv)
{
    int ch;
    unsigned up, down;

    while ((ch = getopt_long(argc, argv, "", rate_opts, NULL)) != -1)
        usage_rate();
    argc -= optind;
    argv += optind;

    if (argc < 2)
        usage_rate();

    up = parse_rate(argv[0]);
    down = parse_rate(argv[1]);

    btpd_connect();
    handle_ipc_res(btpd_rate(ipc, up, down), "rate", argv[1]);
}

/* start section */
void
usage_start(void)
{
    printf(
        "Activate torrents.\n"
        "\n"
        "Usage: start torrent ...\n"
        "\n"
        "Arguments:\n"
        "torrent ...\n"
        "\tThe torrents to activate.\n"
        "\n"
        "Options:\n"
        "-a\n"
        "\tActivate all inactive torrents.\n"
        "\n"
        );
    exit(1);
}

static struct option start_opts [] = {
    { "help", no_argument, NULL, 'H' },
    {NULL, 0, NULL, 0}
};

void
cmd_start(int argc, char **argv)
{
    int ch, all = 0;
    struct ipc_torrent t;

    while ((ch = getopt_long(argc, argv, "a", start_opts, NULL)) != -1) {
        switch (ch) {
        case 'a':
            all = 1;
            break;
        default:
            usage_start();
        }
    }
    argc -= optind;
    argv += optind;

    if ((argc == 0 && !all) || (all && argc != 0))
        usage_start();

    btpd_connect();
    if (all) {
        enum ipc_err code = btpd_start_all(ipc);
        if (code != IPC_OK)
            diemsg("command failed (%s).\n", ipc_strerror(code));
    } else {
       for (int i = 0; i < argc; i++)
           if (torrent_spec(argv[i], &t))
               handle_ipc_res(btpd_start(ipc, &t), "start", argv[i]);
    }
}

/* stat section */
void
usage_stat(void)
{
    printf(
        "Display stats for active torrents.\n"
        "\n"
        "Usage: stat [-i] [-w seconds] [torrent ...]\n"
        "\n"
        "Arguments:\n"
        "torrent ...\n"
        "\tOnly display stats for the given torrents.\n"
        "\n"
        "Options:\n"
        "-i\n"
        "\tDisplay individual lines for each torrent.\n"
        "\n"
        "-n\n"
        "\tDisplay the name of each torrent. Implies '-i'.\n"
        "\n"
        "-w n\n"
        "\tDisplay stats every n seconds.\n"
        "\n");
    exit(1);
}

struct btstat {
    unsigned num;
    enum ipc_tstate state;
    unsigned peers, tr_good;
    long long content_got, content_size, downloaded, uploaded, rate_up,
        rate_down, tot_up;
    uint32_t pieces_seen, torrent_pieces;
};

struct cbarg {
    int individual, names;
    struct btstat tot;
};

static enum ipc_tval stkeys[] = {
    IPC_TVAL_STATE,
    IPC_TVAL_NUM,
    IPC_TVAL_NAME,
    IPC_TVAL_PCOUNT,
    IPC_TVAL_TRGOOD,
    IPC_TVAL_PCCOUNT,
    IPC_TVAL_PCSEEN,
    IPC_TVAL_SESSUP,
    IPC_TVAL_SESSDWN,
    IPC_TVAL_TOTUP,
    IPC_TVAL_RATEUP,
    IPC_TVAL_RATEDWN,
    IPC_TVAL_CGOT,
    IPC_TVAL_CSIZE
};

#define NSTKEYS ARRAY_COUNT(stkeys)

static void
print_stat(struct btstat *st)
{
    print_percent(st->content_got, st->content_size);
    print_size(st->downloaded);
    print_rate(st->rate_down);
    print_size(st->uploaded);
    print_rate(st->rate_up);
    print_ratio(st->tot_up, st->content_size);
    printf("%4u ", st->peers);
    print_percent(st->pieces_seen, st->torrent_pieces);
    printf("%3u", st->tr_good);
    printf("\n");
}

static void
stat_cb(int obji, enum ipc_err objerr, struct ipc_get_res *res, void *arg)
{
    struct cbarg *cba = arg;
    struct btstat st, *tot = &cba->tot;
    if (objerr != IPC_OK || res[IPC_TVAL_STATE].v.num == IPC_TSTATE_INACTIVE)
        return;
    bzero(&st, sizeof(st));
    st.state = res[IPC_TVAL_STATE].v.num;
    st.num = res[IPC_TVAL_NUM].v.num;
    tot->torrent_pieces += (st.torrent_pieces = res[IPC_TVAL_PCCOUNT].v.num);
    tot->pieces_seen += (st.pieces_seen = res[IPC_TVAL_PCSEEN].v.num);
    tot->content_got += (st.content_got = res[IPC_TVAL_CGOT].v.num);
    tot->content_size += (st.content_size = res[IPC_TVAL_CSIZE].v.num);
    tot->downloaded += (st.downloaded = res[IPC_TVAL_SESSDWN].v.num);
    tot->uploaded += (st.uploaded = res[IPC_TVAL_SESSUP].v.num);
    tot->rate_down += (st.rate_down = res[IPC_TVAL_RATEDWN].v.num);
    tot->rate_up += (st.rate_up = res[IPC_TVAL_RATEUP].v.num);
    tot->peers += (st.peers = res[IPC_TVAL_PCOUNT].v.num);
    tot->tot_up += (st.tot_up = res[IPC_TVAL_TOTUP].v.num);
    tot->tr_good += (st.tr_good = res[IPC_TVAL_TRGOOD].v.num);
    if (cba->individual) {
        if (cba->names)
            printf("%.*s\n", (int)res[IPC_TVAL_NAME].v.str.l,
                res[IPC_TVAL_NAME].v.str.p);
        printf("%4u %c. ", st.num, tstate_char(st.state));
        print_stat(&st);
    }
}

static void
do_stat(int individual, int names, int seconds, struct ipc_torrent *tps,
    int ntps)
{
    enum ipc_err err;
    struct cbarg cba;
    int header = 1;
    if (names)
        individual = 1;
    cba.individual = individual;
    cba.names = names;
again:
    header--;
    if (header == 0) {
        if (individual) {
            header = 1;
            printf(" NUM ST ");
        } else
            header = 20;
        printf("  HAVE   DLOAD      RTDWN   ULOAD       RTUP   RATIO CONN"
            "  AVAIL  TR\n");
    }

    bzero(&cba.tot, sizeof(cba.tot));
    cba.tot.state = IPC_TSTATE_INACTIVE;
    if (tps == NULL)
        err = btpd_tget_wc(ipc, IPC_TWC_ACTIVE, stkeys, NSTKEYS,
            stat_cb, &cba);
    else
        err = btpd_tget(ipc, tps, ntps, stkeys, NSTKEYS, stat_cb, &cba);
    if (err != IPC_OK)
        diemsg("command failed (%s).\n", ipc_strerror(err));
    if (names)
        printf("-------\n");
    if (individual)
        printf("        ");
    print_stat(&cba.tot);
    if (seconds > 0) {
        sleep(seconds);
        goto again;
    }
}

static struct option stat_opts [] = {
    { "help", no_argument, NULL, 'H' },
    {NULL, 0, NULL, 0}
};

void
cmd_stat(int argc, char **argv)
{
    int ch;
    int wflag = 0, iflag = 0, nflag = 0, seconds = 0;
    struct ipc_torrent *tps = NULL;
    int ntps = 0;
    char *endptr;
    while ((ch = getopt_long(argc, argv, "inw:", stat_opts, NULL)) != -1) {
        switch (ch) {
        case 'i':
            iflag = 1;
            break;
        case 'n':
            nflag = 1;
            break;
        case 'w':
            wflag = 1;
            seconds = strtol(optarg, &endptr, 10);
            if (*endptr != '\0' || seconds < 1)
                usage_stat();
            break;
        default:
            usage_stat();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc > 0) {
        tps = malloc(argc * sizeof(*tps));
        for (int i = 0; i < argc; i++) {
            if (torrent_spec(argv[i], &tps[ntps]))
                ntps++;
            else
                exit(1);

        }
    }
    btpd_connect();
    do_stat(iflag, nflag, seconds, tps, ntps);
}

/* stop section */
void
usage_stop(void)
{
    printf(
        "Deactivate torrents.\n"
        "\n"
        "Usage: stop -a\n"
        "       stop torrent ...\n"
        "\n"
        "Arguments:\n"
        "torrent ...\n"
        "\tThe torrents to deactivate.\n"
        "\n"
        "Options:\n"
        "-a\n"
        "\tDeactivate all active torrents.\n"
        "\n"
        );
    exit(1);
}

static struct option stop_opts [] = {
    { "help", no_argument, NULL, 'H' },
    {NULL, 0, NULL, 0}
};

void
cmd_stop(int argc, char **argv)
{
    int ch, all = 0;
    struct ipc_torrent t;

    while ((ch = getopt_long(argc, argv, "a", stop_opts, NULL)) != -1) {
        switch (ch) {
        case 'a':
            all = 1;
            break;
        default:
            usage_stop();
        }
    }
    argc -= optind;
    argv += optind;

    if ((argc == 0 && !all) || (all && argc != 0))
        usage_stop();

    btpd_connect();
    if (all) {
        enum ipc_err code = btpd_stop_all(ipc);
        if (code != IPC_OK)
            diemsg("command failed (%s).\n", ipc_strerror(code));
    } else {
        for (int i = 0; i < argc; i++)
            if (torrent_spec(argv[i], &t))
                handle_ipc_res(btpd_stop(ipc, &t), "stop", argv[i]);
    }
}
