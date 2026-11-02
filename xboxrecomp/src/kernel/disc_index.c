/*
 * disc_index.c -- sheroes-nx: an in-memory list of the disc's files (Switch).
 *
 * Sonic Heroes opens every file on its disc at boot (about 3,900 of them,
 * NtCreateFile after NtCreateFile from 12 s to 36 s in the Switch log of
 * 2026-11-01, NRO f42470f) and keeps them open, after listing every folder
 * with NtQueryDirectoryFile (1-12 s). On Horizon each open() is a round trip
 * to the SD layer, and stat() is worse: newlib's fsdev opens the file to read
 * its size. So the boot spent half a minute on the SD card before the first
 * picture.
 *
 * This walks <game_dir>/dvd once with the native directory reads, which hand
 * back each entry's type and size in batches, and answers stat() for that
 * subtree from memory. kernel_file.c uses it to:
 *   - give a read-only open of a listed file a handle with no descriptor yet
 *     (w32_open_handle_lazy): the file is really opened on its first read;
 *   - answer NtQueryInformationFile, NtQueryFullAttributesFile and each entry
 *     of NtQueryDirectoryFile without touching the SD card.
 * Only the dvd folder: the game never writes there. TDATA/UDATA (saves) live
 * in the game dir too and always go to the real filesystem.
 *
 * RECOMP_DISC_INDEX=0 turns it off. Other platforms: stubs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int disc_index_stat(const char *host_path, struct stat *st);
void disc_index_set_root(const char *game_dir);

#if defined(__SWITCH__)
#include <switch.h>
#include <ctype.h>
#include <pthread.h>

typedef struct {
    char    *key;       /* lower-case path below the root, "/"-separated */
    int64_t  size;
    int      is_dir;
} di_entry;

static char       s_root[512];       /* "sdmc:/switch/sheroes/game/dvd", lower case */
static size_t     s_root_len;
static int        s_state;           /* 0 not built, 1 built, -1 off / failed */
static di_entry  *s_tab;
static size_t     s_cap, s_count;
static time_t     s_time;
static pthread_mutex_t s_mx = PTHREAD_MUTEX_INITIALIZER;

static uint32_t hash_str(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

static void tab_put(const char *key, int64_t size, int is_dir)
{
    if ((s_count + 1) * 2 > s_cap) {
        size_t ncap = s_cap ? s_cap * 2 : 8192, i;
        di_entry *n = calloc(ncap, sizeof *n);
        if (!n) return;
        for (i = 0; i < s_cap; i++)
            if (s_tab[i].key) {
                size_t j = hash_str(s_tab[i].key) & (ncap - 1);
                while (n[j].key) j = (j + 1) & (ncap - 1);
                n[j] = s_tab[i];
            }
        free(s_tab);
        s_tab = n;
        s_cap = ncap;
    }
    size_t j = hash_str(key) & (s_cap - 1);
    while (s_tab[j].key) {
        if (!strcmp(s_tab[j].key, key)) return;
        j = (j + 1) & (s_cap - 1);
    }
    s_tab[j].key = strdup(key);
    s_tab[j].size = size;
    s_tab[j].is_dir = is_dir;
    s_count++;
}

static const di_entry *tab_get(const char *key)
{
    if (!s_cap) return NULL;
    size_t j = hash_str(key) & (s_cap - 1);
    while (s_tab[j].key) {
        if (!strcmp(s_tab[j].key, key)) return &s_tab[j];
        j = (j + 1) & (s_cap - 1);
    }
    return NULL;
}

static void lower_into(char *dst, const char *src, size_t cap)
{
    size_t i = 0;
    for (; *src && i + 1 < cap; src++) {
        char c = *src == '\\' ? '/' : (char)tolower((unsigned char)*src);
        if (c == '/' && i && dst[i - 1] == '/') continue;    /* "//" -> "/" */
        dst[i++] = c;
    }
    while (i > 1 && dst[i - 1] == '/') i--;                    /* no trailing "/" */
    dst[i] = '\0';
}

/* Walk fs_path (device-relative, e.g. "/switch/sheroes/game/dvd"); key is the
 * lower-case path below the root ("" for the root itself). */
static int walk(FsFileSystem *fs, const char *fs_path, const char *key, int depth)
{
    static FsDirectoryEntry ents[128];
    FsDir d;
    s64 n = 0;
    char (*subs)[FS_MAX_PATH] = NULL;
    int nsub = 0, capsub = 0, dirs = 1, i;
    if (depth > 16 || R_FAILED(fsFsOpenDirectory(fs, fs_path,
            FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles, &d)))
        return 0;
    while (R_SUCCEEDED(fsDirRead(&d, &n, 128, ents)) && n > 0) {
        for (i = 0; i < n; i++) {
            char k[FS_MAX_PATH], low[FS_MAX_PATH];
            int is_dir = ents[i].type == FsDirEntryType_Dir;
            lower_into(low, ents[i].name, sizeof low);
            snprintf(k, sizeof k, "%s/%s", key, low);
            tab_put(k, is_dir ? 0 : ents[i].file_size, is_dir);
            if (is_dir) {
                if (nsub == capsub) {
                    capsub = capsub ? capsub * 2 : 16;
                    subs = realloc(subs, (size_t)capsub * sizeof *subs);
                    if (!subs) { fsDirClose(&d); return dirs; }
                }
                snprintf(subs[nsub++], FS_MAX_PATH, "%s", ents[i].name);
            }
        }
    }
    fsDirClose(&d);                  /* one open directory at a time */
    for (i = 0; i < nsub; i++) {
        char p[FS_MAX_PATH], k[FS_MAX_PATH], low[FS_MAX_PATH];
        snprintf(p, sizeof p, "%s/%s", fs_path, subs[i]);
        lower_into(low, subs[i], sizeof low);
        snprintf(k, sizeof k, "%s/%s", key, low);
        dirs += walk(fs, p, k, depth + 1);
    }
    free(subs);
    return dirs;
}

void disc_index_set_root(const char *game_dir)
{
    char tmp[512];
    const char *e = getenv("RECOMP_DISC_INDEX");
    if (e && e[0] == '0') { s_state = -1; return; }
    if (!game_dir || strncmp(game_dir, "sdmc:", 5) != 0) { s_state = -1; return; }
    snprintf(tmp, sizeof tmp, "%s/dvd", game_dir);
    lower_into(s_root, tmp, sizeof s_root);
    s_root_len = strlen(s_root);
}

static void build_locked(void)
{
    FsFileSystem *fs = fsdevGetDeviceFileSystem("sdmc");
    struct stat rst;
    u64 t0 = armGetSystemTick();
    char fs_path[FS_MAX_PATH];
    int dirs;
    s_state = -1;
    if (!fs || !s_root_len) return;
    snprintf(fs_path, sizeof fs_path, "%s", s_root + 5);     /* drop "sdmc:" */
    if (stat(s_root, &rst) == 0)
        s_time = rst.st_mtime;
    dirs = walk(fs, fs_path, "", 0);
    if (!s_count) {
        fprintf(stderr, "  [DISC] index of %s: nothing found, using the SD card directly\n", s_root);
        return;
    }
    tab_put("", 0, 1);
    s_state = 1;
    fprintf(stderr, "  [DISC] indexed %s: %zu entries in %d folders, %llu ms\n", s_root,
            s_count, dirs,
            (unsigned long long)(armTicksToNs(armGetSystemTick() - t0) / 1000000ull));
}

int disc_index_stat(const char *host_path, struct stat *st)
{
    char low[FS_MAX_PATH];
    const di_entry *en;
    if (s_state < 0 || !s_root_len || !host_path)
        return -1;
    lower_into(low, host_path, sizeof low);
    if (strncmp(low, s_root, s_root_len) != 0 || (low[s_root_len] && low[s_root_len] != '/'))
        return -1;
    pthread_mutex_lock(&s_mx);
    if (s_state == 0)
        build_locked();
    en = s_state > 0 ? tab_get(low + s_root_len) : NULL;
    pthread_mutex_unlock(&s_mx);
    if (!en)
        return -1;                   /* not listed: let the real filesystem answer */
    memset(st, 0, sizeof *st);
    st->st_mode = en->is_dir ? (S_IFDIR | 0755) : (S_IFREG | 0644);
    st->st_size = en->size;
    st->st_nlink = 1;
    st->st_atime = st->st_mtime = st->st_ctime = s_time;
    return 0;
}

#else

void disc_index_set_root(const char *game_dir) { (void)game_dir; }
int disc_index_stat(const char *host_path, struct stat *st)
{
    (void)host_path; (void)st;
    return -1;
}

#endif
