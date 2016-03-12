#include "judge_main.h"

void error_report(const char s[])
{
    fprintf(stderr,"%s",s);
    exit(1);
}


void system_chmod(char path[],const char a[])
{
    char buf[PATH_MAX];
    sprintf(buf,"chmod %s %s",a,path);
    if(system(buf) != 0)
    {
        error_report("copy file error");
    }
}



void check_file(char path[])
{
    if(access(path,F_OK) != 0)
    {
        error_report("argv file dont exist");
    }
    if(access(path,R_OK) != 0)
        system_chmod(path,"+r");
}


long get_file_size(const char *path)
{
    struct stat f_stat;
    if(stat(path,&f_stat) == -1)
        return 0;
    return (long) f_stat.st_size;
}


void reopen(int fileno,const char* path,int oflag,mode_t mode)
{
    int tmp_fileno = open(path,oflag,mode);
    if(tmp_fileno == -1)
        error_report("open");

    dup2(tmp_fileno,fileno);
    close(tmp_fileno);
}


void exec(long used_time)
{

    nice(20);

    reopen(STDIN_FILENO, input_path, O_RDONLY, 0);

    reopen(STDOUT_FILENO, code_output_path, O_WRONLY | O_CREAT  | O_TRUNC, S_IRWXU | S_IRGRP | S_IROTH);
    reopen(STDERR_FILENO, code_error_path,  O_WRONLY | O_CREAT | O_APPEND, S_IRWXU | S_IRGRP | S_IROTH);


    struct rlimit LIM;

    //CPU用时限制，软->SIGXCPU, 硬SIGKILL
    LIM.rlim_cur = time_limit - used_time/1000;
    LIM.rlim_max = LIM.rlim_cur + 1;
    if(setrlimit(RLIMIT_CPU,&LIM) < 0)
        error_report("setrlimit [CPU]");

    alarm(0); //取消之间，防止sleep恶意运行
    alarm(LIM.rlim_cur * 10);


    //输出限制
    LIM.rlim_cur = 32 * MB;
    LIM.rlim_max = LIM.rlim_cur + MB;
    if(setrlimit(RLIMIT_FSIZE,&LIM) < 0)
        error_report("setrlimit [FSIZE]");


    //进程限制
    if(language == LANG_JAVA)
        LIM.rlim_max = LIM.rlim_cur = 1000;
    else
        LIM.rlim_max = LIM.rlim_cur = 1;
    if(setrlimit(RLIMIT_NPROC,&LIM) < 0)
        error_report("setrlimit [NPROC]");



    //内存限制
    LIM.rlim_cur = memory_limit;
    LIM.rlim_max = memory_limit * 2;
    if(setrlimit(RLIMIT_AS,&LIM) < 0)
        error_report("setrlimit [AS]");


    //堆栈限制
    LIM.rlim_max = LIM.rlim_cur = 64 * MB;
    if(setrlimit(RLIMIT_STACK,&LIM) < 0)
        error_report("setrlimit [STACK]");


    if(ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0)
        error_report("Error ptrace");


    chdir(code_dir);
    if(chroot(code_dir) != 0)
        error_report("Error chroot");

    switch (language)
    {
        case LANG_C: case LANG_CPP:
            execl("./code","./code",(char *)NULL);
           // printf("***** %d\n",execl(code_exec,code_exec, (char *)NULL));
            perror(strerror(errno));
            break;
        case LANG_JAVA:
            execl("java",
                  "java",
                  "-Xms128M",
                  "-Xms512M",
                  "-Djava.security.manager",
                  "-Djava.security.policy=./java.policy",
                  "-DONLINE_JUDGE=true",
                  "Main",
                  (char *)NULL);
            break;
        default:
            error_report("Unknown language");
    }

}







//获取进程状态
long get_proc_status(pid_t pid,const char *mark)
{
    FILE *pf;
    char fn[PATH_MAX+1];
    sprintf(fn,"/proc/%d/status",pid);
    pf = fopen(fn,"r");

    size_t m = strlen(mark);

    char buf[BUFSIZ+1];
    long ret = 0;

    while(pf && fgets(buf,BUFSIZ,pf))
    {
        buf[strlen(buf)-1] = '\0';
        if(strncmp(buf,mark,m) == 0)
        {
            sscanf(buf+m+1,"%ld",&ret);
            break;
        }
    }

    if(pf)
        fclose(pf);

    return ret;
}



//获取页面错误
//java use PAGEFAULT
long get_page_fault_memory(struct rusage &ru,pid_t &pid)
{
    long m_minflt = ru.ru_minflt * getpagesize();

    return m_minflt;
}



void watch(pid_t pid,enum judge_status &judge_status,long &max_used_memory,long &used_time,long error_size)
{
    long init_used_time = used_time;
    long used_memory;

    init_syscall_limit();

    int status;
    struct rusage ru;

    while(1)
    {
        pid_t pid_ret = wait4(pid,&status,0,&ru);
        if(pid_ret != pid)
            error_report("wait4");

        //TLE
        used_time = init_used_time + ru.ru_utime.tv_sec * 100 + ru.ru_utime.tv_usec / 1000;
        if(used_time >= time_limit * 1000)
        {
            judge_status = JUDGE_TLE;
            goto send_kill;
        }

        //MLE
        if (language == LANG_JAVA)
        {
            // JVM GC ask VM before need, so used kernel page fault times and page size.
            used_memory = get_page_fault_memory(ru, pid);
        } else
        {
            // other use VmPeak
            used_memory = get_proc_status(pid, "VmPeak:") << 10;
        }

        if(used_memory > max_used_memory)
            max_used_memory = used_memory;
        if(max_used_memory > memory_limit)
        {
            judge_status = JUDGE_MLE;
            goto send_kill;
        }


        //RE 判断stderr
        if(get_file_size(code_error_path) > error_size)
        {
            judge_status = JUDGE_RE;
            goto send_kill;
        }

        //进程退出
        if(WIFEXITED(status))
        {
            int exitcode = WEXITSTATUS(status);
            if(exitcode != 0)
            {
                judge_status = JUDGE_RE;
            }
            break;
        }

        //未捕捉到信号而中止
        if(WIFSIGNALED(status))
        {
            int sigcode = WTERMSIG(status);
            break;
        }

        // 未捕捉信号而暂停
        // 子进程被追踪(ptrace)或调用WUN-TRACED时才可能发生
        if (WIFSTOPPED(status))
        {

            int sigcode = WSTOPSIG(status);

            switch (sigcode) {
                case SIGSTOP:
                    // JAVA 使用 SIGSTOP 等待下次 CPU 运行
                    if (language != LANG_JAVA) {
                        judge_status = JUDGE_RE;
                        goto send_kill;
                    }
                case SIGTRAP:
                    // c++ case 语句中不能出现带初始化的变量声明
                    unsigned long syscall;
                    syscall = get_syscall(pid);
                    //printf("System call: %lu\n", syscall);
                    switch (check_syscall(syscall)) {
                        case 0:

                            judge_status = JUDGE_RE;
                            goto send_kill;
                        case 1:
                            ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
                            break;
                        case 2:
                            //ptrace(PTRACE_SETREGS, pid, NULL, NULL);
                            break;
                    }

                    break;
                case SIGSEGV:
                    judge_status = JUDGE_MLE;
                    goto send_kill;
                case SIGCHLD:
                case SIGALRM:
                    alarm(0);
                case SIGXCPU:
                case SIGKILL:
                    judge_status = JUDGE_TLE;
                    goto send_kill;
                case SIGXFSZ:
                    judge_status = JUDGE_OLE;
                    goto send_kill;
                default:
                    judge_status = JUDGE_SE;
                    goto send_kill;
            }

        }
    }

    return;
    send_kill:
        ptrace(PTRACE_KILL,pid,NULL,NULL);
}




inline void trim(string &str)
{
    while(str.length() && ( *(str.end()-1) == ' ' || *(str.end()-1) == '\n'))
        str.erase(str.end()-1);
}

enum judge_status check_user_output()
{
    struct stat st,ss;
    string strt,strs;
    stat(code_output_path,&st);
    stat(input_path,&ss);

    if(st.st_size < ss.st_size / 2 || st.st_size > ss.st_size * 2) return JUDGE_WA; //accelerate
    bool flag = false;
    ifstream fint(code_output_path,ifstream::in), fins(input_path,ifstream::in);


    while(fint.good() || fins.good())
    {
        if(fint.good())
            getline(fint,strt);
        else strt = "";

        if(fins.good())
            getline(fins,strs);
        else strs = "";

        if(strt != strs)
        {
            trim(strt);
            trim(strs);
            if(strt == strs)
                flag = true;
            else
                return JUDGE_WA;
        }
    }
    if(flag)
        return JUDGE_PE;
    return JUDGE_AC;
}

void compile()
{

    switch(language)
    {
        case LANG_C:
            sprintf(code_exec,"%s/%ld/code",RUN_DIR,run_id);
            char buf[10000];
            memset(buf,0,sizeof(buf));
            sprintf(buf,"gcc -o %s %s  -static",code_exec,code_path);
            if(system(buf) != 0)
                error_report("Compile Error");
            break;
        case LANG_CPP:
            break;
        case LANG_JAVA:
            break;
        default:
            break;
    }

}

void init()
{
    sprintf(input_dir,"%s/%ld/input",DATA_DIR,problem_id);
    sprintf(output_dir,"%s/%ld/output",DATA_DIR,problem_id);

    sprintf(code_dir,"%s/%ld",RUN_DIR,run_id);
    sprintf(code_path,"%s/%ld/code.%s",RUN_DIR,run_id,language_suffix[language]);
    sprintf(code_error_path,"%s/%ld/error",RUN_DIR,run_id);
    compile();
    struct passwd * child_user = getpwnam(JUDGE_USER_NAME);
    if(child_user == NULL)
        error_report("Can't find this user");
    child_uid = child_user->pw_uid;
    child_gid = child_user->pw_gid;

    system_chmod(code_dir,"711");
    system_chmod(code_exec,"711");
    system_chmod(code_path,"666");
//    open(code_error_path,O_CREAT,S_IRUSR);
//    system_chmod(code_error_path,"666");
//    chown(code_path,child_uid,child_uid);
//    chown(code_exec,child_uid,child_uid);
}


int prepare(char *file_name)
{

    int len = strlen(file_name);
    if(len != 8 || strncmp(file_name,"input",5) != 0)
        return 0;
    int id = atoi(file_name + len-3);
    //printf("**** %03d\n",id);

    sprintf(input_path,"%s/input%03d",input_dir,id);
    sprintf(output_path,"%s/output%03d",output_dir,id);
    sprintf(code_output_path,"%s/code_output%03d",code_dir,id);

    check_file(input_path);
    check_file(output_path);
//    open(code_output_path,O_CREAT,S_IRUSR);
//    system_chmod(code_output_path,"666");

  //  printf("%s   %s   %s\n",input_path,output_path,code_output_path);

    return 1;
}


int main(int argc,char *argv[])
{


    parent_uid = geteuid();
    parent_gid = getegid();

    if(argc != 6)
    {
        error_report(" param: [run_id] [problem_id] [language] [time_limit] [memory_limit] ");
    }
    if(parent_uid != 0)
    {
        error_report("Error: judger should be run as root.");
    }

    run_id = atoi(argv[1]);
    problem_id = atoi(argv[2]);
    language = atoi(argv[3]);
    time_limit = atoi(argv[4]);
    memory_limit = atoi(argv[5])*MB;

    init();

    DIR *dp;
    struct dirent *dirp;
    if((dp = opendir(input_dir)) == NULL)
    {
        error_report("Can't open input dir");
    }

    enum judge_status judge_status = JUDGE_RJ;
    long used_time = 0; //ms
    long max_used_memory = 0; //B
    long error_size = 0; // B
    int test_count = 0;


    while(judge_status == JUDGE_RJ && (dirp = readdir(dp)) != NULL)
    {

        if(!prepare(dirp->d_name)) continue;

        error_size = get_file_size(code_error_path);

        child_pid = fork();


        if(child_pid == 0)
        {
            exec(used_time);
            exit(0);
        }
        else
        {
            watch(child_pid,judge_status,max_used_memory,used_time,error_size);

            if(judge_status == JUDGE_RJ)
                judge_status = check_user_output();

            test_count += 1;
        }

    }
    if(judge_status == JUDGE_RJ)
        judge_status = JUDGE_AC;
    printf("%d  %ld  %ld\n",judge_status,used_time,max_used_memory >> 10);// ms kB

    return 0;
}





