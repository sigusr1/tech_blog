#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <sys/capability.h>

#define MASTER_PROCESS_UID ("master")
#define SLAVE_PROCESS_UID ("slave")
#define TEST_FILE_PATH ("/home/huo/test.txt")

#define gettid() syscall(SYS_gettid)
#define try_open_file()                                                                                                            \
    {                                                                                                                              \
        FILE *fp = fopen(TEST_FILE_PATH, "r");                                                                                     \
        if (fp)                                                                                                                    \
        {                                                                                                                          \
            printf("%s:%d pid:%u tid:%ld Success to open file.\n", __func__, __LINE__, getpid(), gettid());                        \
            fclose(fp);                                                                                                            \
        }                                                                                                                          \
        else                                                                                                                       \
        {                                                                                                                          \
            printf("%s:%d pid:%u tid:%ld Fail to open file. error:%s\n", __func__, __LINE__, getpid(), gettid(), strerror(errno)); \
        }                                                                                                                          \
    }

int set_reserve_caps()
{
    pid_t pid;
    cap_t cap;

    cap_value_t cap_list[CAP_LAST_CAP + 1];
    cap_flag_t cap_flags;
    cap_flag_value_t cap_flags_value;

    pid = getpid();
    cap = cap_get_pid(pid);
    if (NULL == cap)
    {
        printf("%s:%d pid:%u tid:%ld cap_get_pid err:%s\n", __func__, __LINE__, getpid(), gettid(), strerror(errno));
        return -1;
    }

    // First clear all caps
    if (cap_clear(cap) != 0)
    {
        printf("%s:%d pid:%u tid:%ld cap_clear err:%s\n", __func__, __LINE__, getpid(), gettid(), strerror(errno));
        cap_free(cap);
        return -1;
    }

    // Then set reserve caps, follow caps will be kept after change uid
    cap_list[0] = CAP_SETPCAP;
    cap_list[1] = CAP_DAC_OVERRIDE;
    cap_list[2] = CAP_SETUID;

    if (cap_set_flag(cap, CAP_PERMITTED, 3, cap_list, CAP_SET) == -1)
    {
        printf("%s:%d pid:%u tid:%ld set CAP_PERMITTED err:%s\n", __func__, __LINE__, getpid(), gettid(), strerror(errno));
        cap_free(cap);
        return -1;
    }

    if (cap_set_flag(cap, CAP_EFFECTIVE, 3, cap_list, CAP_SET) == -1)
    {
        printf("%s:%d pid:%u tid:%ld set CAP_EFFECTIVE err:%s\n", __func__, __LINE__, getpid(), gettid(), strerror(errno));
        cap_free(cap);
        return -1;
    }

    int ret = cap_set_proc(cap);
    if (ret != 0)
    {
        printf("%s:%d pid:%u tid:%ld cap_set_proc err:%s\n", __func__, __LINE__, getpid(), gettid(), strerror(errno));
        cap_free(cap);
        return -1;
    }

    cap_free(cap);
    return 0;
}

int ctrl_reserve_caps(int enable)
{
    pid_t pid;
    cap_t cap;

    cap_value_t cap_list[CAP_LAST_CAP + 1];
    cap_flag_t cap_flags;
    cap_flag_value_t cap_flags_value;

    pid = getpid();
    cap = cap_get_pid(pid);
    if (NULL == cap)
    {
        printf("%s:%d pid:%u tid:%ld cap_get_pid err:%s\n", __func__, __LINE__, getpid(), gettid(), strerror(errno));
        return -1;
    }

    cap_list[0] = CAP_SETPCAP;
    cap_list[1] = CAP_DAC_OVERRIDE;
    cap_list[2] = CAP_SETUID;

    int ctrl_cmd;
    if (enable)
    {
        ctrl_cmd = CAP_SET;
    }
    else
    {
        ctrl_cmd = CAP_CLEAR;
    }

    if (cap_set_flag(cap, CAP_EFFECTIVE, 3, cap_list, ctrl_cmd) == -1)
    {
        printf("%s:%d pid:%u tid:%ld ctrl_cmd:%d err:%s\n", __func__, __LINE__, getpid(), gettid(), ctrl_cmd, strerror(errno));
        cap_free(cap);
        return -1;
    }

    int ret = cap_set_proc(cap);
    if (ret != 0)
    {
        printf("%s:%d pid:%u tid:%ld cap_set_proc err:%s\n", __func__, __LINE__, getpid(), gettid(), strerror(errno));
        cap_free(cap);
        return -1;
    }

    cap_free(cap);
    return 0;
}

int get_uid_by_name(const char *uid_name)
{
    if (NULL == uid_name)
    {
        return -1;
    }

    struct passwd *pw = getpwnam(uid_name);
    if (NULL == pw)
    {
        printf("%s:%d pid:%u tid:%ld getpwnam:%s err:%s\n", __func__, __LINE__, getpid(), gettid(), uid_name, strerror(errno));
        return -1;
    }

    return pw->pw_uid;
}

int change_uid(const char *uid_name)
{
    int uid = get_uid_by_name(uid_name);
    if (uid < 0)
    {
        return -1;
    }

    // Keep caps after change uid (Only CAP_PERMITTED caps kept)
    if (prctl(PR_SET_KEEPCAPS, 1) < 0)
    {
        printf("%s:%d pid:%u tid:%ld prctl:%s err:%s\n", __func__, __LINE__, getpid(), gettid(), uid_name, strerror(errno));
        return -1;
    }

    if (setreuid(uid, uid) < 0)
    {
        printf("%s:%d pid:%u tid:%ld uid_name:%s err:%s\n", __func__, __LINE__, getpid(), gettid(), uid_name, strerror(errno));
        return -1;
    }
    return 0;
}

static void *test_in_child_thread(void *arg)
{
    while (1)
    {
        try_open_file();
        sleep(3);
    }
}

int main(void)
{
    // Just keep caps follow experiments need
    if (set_reserve_caps() < 0)
    {
        return -1;
    }

    if (change_uid(MASTER_PROCESS_UID) < 0)
    {
        return -1;
    }

    // Open file fail because CAP_EFFECTIVE is clear after change uid from root(0) to nonzero
    try_open_file();

    // Enable reserved caps
    if (ctrl_reserve_caps(1) < 0)
    {
        return -1;
    }

    // Open file success becuase restore CAP_EFFECTIVE caps
    try_open_file();

    if (fork() == 0) // Child process
    {
        // Open file success because child process inherit all caps from father process
        try_open_file();

        if (change_uid(SLAVE_PROCESS_UID) < 0)
        {
            return -1;
        }
        // Open file success because all caps kept after change uid from nonzero to other
        try_open_file();

        // This thread inherit caps from main thread, so could open file
        pthread_t always_success_thread;
        pthread_create(&always_success_thread, NULL, &test_in_child_thread, NULL);

        // Because clear caps only affect call thread, so always_success_thread won't be affect
        if (ctrl_reserve_caps(0) < 0)
        {
            return -1;
        }

        // Open file fail because clear CAP_EFFECTIVE caps
        try_open_file();

        // Inherit caps from main thread, but now main thread can't open file, so you should fail too.
        pthread_t always_fail_thread;
        pthread_create(&always_fail_thread, NULL, &test_in_child_thread, NULL);
    }

    pause();
    return 0;
}