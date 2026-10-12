#include "Protocol.h"
#include "doomgeneric.h"
#include "doomkeys.h"
#ifdef _WIN32
#include <io.h>
#include <wchar.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static MDShared *shared;
#ifdef _WIN32
static HANDLE parent_handle;
#else
static pid_t parent;
#endif
static uint32_t delivered, sequence;
static uint64_t last_time, game_ns = 1000000;
static const unsigned char keys[MD_KEYS] = {
    KEY_UPARROW, KEY_DOWNARROW, KEY_LEFTARROW, KEY_RIGHTARROW,
    KEY_STRAFE_L, KEY_STRAFE_R, KEY_FIRE, KEY_USE, KEY_RSHIFT,
    KEY_ESCAPE, KEY_ENTER, KEY_TAB, '1', '2', '3', '4', '5', '6', '7', 'y', 'n', KEY_EQUALS, KEY_MINUS
};
static void check_parent(void) {
#ifdef _WIN32
    if (WaitForSingleObject(parent_handle,0)!=WAIT_TIMEOUT || md_load32(&shared->quit)) ExitProcess(0);
#else
    if (getppid() != parent || md_load32(&shared->quit)) _exit(0);
#endif
}
static int active(void) {
    return !md_load32(&shared->paused)
        && md_now() - md_load64(&shared->heartbeat) < 500000000ull;
}
void DG_Init(void) { last_time = md_now(); }
void DG_DrawFrame(void) {
    check_parent();
    ++sequence;
    for (int i = 0; i < 3; ++i) {
        MDFrame *f = &shared->frames[i];
        if (!md_claim(&f->state, 0, 1)) continue;
        memcpy(f->pixels, DG_ScreenBuffer, sizeof(f->pixels));
        f->sequence = sequence;
        md_store32(&f->state, 2);
        break;
    }
    // No spinning at thousands of redraws per second between game tics.
    md_sleep(1);
}
void DG_SleepMs(uint32_t ms) {
    check_parent();
    md_sleep(ms > 20 ? 20 : ms);
}
uint32_t DG_GetTicksMs(void) {
    check_parent();
    uint64_t now = md_now();
    if (active()) game_ns += now - last_time;
    last_time = now;
    return (uint32_t)(game_ns / 1000000ull);
}
int DG_GetKey(int *pressed, unsigned char *key) {
    uint32_t wanted = active() ? md_load32(&shared->keys) : 0;
    uint32_t changes = wanted ^ delivered;
    if (!changes) return 0;
    unsigned bit = (unsigned)__builtin_ctz(changes);
    *pressed = !!(wanted & (1u << bit));
    *key = keys[bit];
    delivered ^= 1u << bit;
    return 1;
}
void DG_SetWindowTitle(const char *title) { (void)title; }

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    if (argc!=4) return 2;
    parent_handle=OpenProcess(SYNCHRONIZE,FALSE,wcstoul(argv[3],NULL,10));
    if (!parent_handle) return 3;
    HANDLE file=CreateFileW(argv[1],GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    LARGE_INTEGER size;
    if (file==INVALID_HANDLE_VALUE || !GetFileSizeEx(file,&size) || size.QuadPart!=sizeof(MDShared)) return 3;
    HANDLE mapping=CreateFileMappingW(file,NULL,PAGE_READWRITE,0,0,NULL);
    if (!mapping) return 4;
    shared=(MDShared*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(MDShared));
    CloseHandle(file);
    if (!shared || shared->magic!=MD_MAGIC) return 4;
    wchar_t *slash=wcsrchr(argv[1],L'\\');
    if (!slash) slash=wcsrchr(argv[1],L'/');
    if (!slash) return 5;
    *slash=L'\0';
    if (!SetCurrentDirectoryW(argv[1])) return 5;
    freopen("engine.log","w",stdout);
    _dup2(_fileno(stdout),_fileno(stderr));
    setvbuf(stdout,NULL,_IONBF,0);
    int bytes=WideCharToMultiByte(CP_UTF8,0,argv[2],-1,NULL,0,NULL,NULL);
    char *wad=(char*)malloc(bytes);
    if (!wad || !WideCharToMultiByte(CP_UTF8,0,argv[2],-1,wad,bytes,NULL,NULL)) return 5;
    char *args[]={"DoomEngine.exe","-iwad",wad,"-nosound","-nogui",
        "-config","NUL","-extraconfig","NUL","-skill","3","-warp","1","1",NULL};
#else
int main(int argc, char **argv) {
    if (argc != 4) return 2;
    parent = (pid_t)strtol(argv[3], NULL, 10);
    int fd = open(argv[1], O_RDWR);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) || st.st_size != sizeof(MDShared)) return 3;
    shared = mmap(NULL, sizeof(MDShared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (shared == MAP_FAILED || shared->magic != MD_MAGIC) return 4;
    // All engine config/save files are confined to this disposable instance directory.
    char *slash = strrchr(argv[1], '/');
    if (!slash) return 5;
    *slash = '\0';
    if (chdir(argv[1])) return 5;
    *slash = '/';
    char *args[] = {argv[0], "-iwad", argv[2], "-nosound", "-nogui",
        "-config", "/dev/null", "-extraconfig", "/dev/null",
        "-skill", "3", "-warp", "1", "1", NULL};
#endif
    doomgeneric_Create(14, args);
    for (;;) {
        // The engine's virtual clock freezes when paused or not being rendered.
        // Its own wait loops still check parent liveness via DG_SleepMs.
        check_parent();
        doomgeneric_Tick();
    }
}
