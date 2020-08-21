#include<unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#define MAX_CMD_LEN 256
#define MAX_CMD_NUM 32
#define MAX_PARA_NUM 256
#define MAX_VAR_LENTH 256
#define MAXBUFSIZE 1024
#define Quit  -66
#define EXIT  -66
#define INS_DONE 1
#define KEY_MEM 1234
#define MAX_PATH_LEN 1024
#define UNINTER -55
#define DEFAULT_PID -1
enum Job_Cond {
	STOPPED, RUNNING, FINISH
};
typedef struct job_node* PtrToJob;
struct job_node
{
	pid_t pid;
	char jobname[MAX_CMD_LEN];
	Job_Cond condition;
	PtrToJob lastJob;
	PtrToJob nextJob;
	bool BGFG;//bg-->0, fg -->1
	int idx;
};
typedef struct Jobs* JobList;
struct Jobs
{
	int job_num;//所有的job数
	PtrToJob FirstJob;//最新的当前存在的后台进程
	PtrToJob LastJob;//最旧的当前存在的后台进程
};
pid_t wait_pid = DEFAULT_PID;
JobList joblist = (JobList)malloc(sizeof(struct Jobs));
typedef struct var_node* PtrToVar;
typedef struct var_node
{
	char var_name[MAX_VAR_LENTH];//变量名称
	char var_value[MAX_CMD_LEN];//变量值
};
PtrToVar ptr_var_head;//变量头
char CUR_ABSO_PATH[MAXBUFSIZE];//当前绝对路径
int pipeFd[2];
typedef struct cmd_paras* PtrToParas;
typedef struct cmd_paras
{
	char para_value[MAX_PARA_NUM];//paravalue初始化为0
	PtrToParas next_para;//指向下一个para
};

typedef struct cmd_attr* PtrToCmd;//命令列表
 struct cmd_attr//命令本身
{
	char* cmd_name;//命令名称
	PtrToParas cmd_paras;//参数数组
	char* cmd_postfix;//命令后缀
	int bg_flag;//后台运行标志 0->no, 1->yes
	int pipe_in_flag;//管道输入标志 0->no, 1->yes
	int pipe_out_flag;//管道输出标志 0->no, 1->yes
	int redirect_int_flag; //重定向输入标志 - 1->no, 0->cover, 1->append;
	int redirect_out_flag;//重定向输出标志，-1->no,0->cover, 1->append ;
	char* redirect_file_in;//重定向输入文件名
	char* redirect_file_out;//重定向输出文件名
	int test_flag;
	int para_num;//参数个数
	PtrToCmd nextcmd;
};
 typedef struct cmd_line* PtrToCmdLine;
 struct cmd_line
 {
	 PtrToCmd cmd_line[MAX_CMD_NUM];
	 int cmd_cnt;
 };
PtrToJob creat_new_job()//设置默认值
{
	PtrToJob new_job = (PtrToJob)malloc(sizeof(struct job_node));
	new_job->lastJob = NULL;
	new_job->condition = RUNNING;
	new_job->BGFG = 1;//默认为fg
	new_job->nextJob = NULL;
	new_job->pid = 0;
	return new_job;
}
PtrToJob set_job(pid_t pid, char* name, int type, Job_Cond condition)//根据传入参数修改进程属性
{
	PtrToJob new_job = creat_new_job();
	new_job->pid = pid;
	new_job->BGFG = type;
	new_job->condition = condition;
	strcpy(new_job->jobname, name);
	return new_job;
}
void unset_job(pid_t pid)//删除已完成进程
{
	PtrToJob job_temp = joblist->FirstJob;
	PtrToJob job_pre = NULL;
	while (job_temp != NULL)
	{ 
		job_pre = job_temp;
		if (job_temp->pid == pid)
			break;
		job_temp = job_temp->nextJob;
	}
	job_temp->condition = FINISH;

}
PtrToJob find_job(int num)//根据进程序号寻找特定进程
{
	PtrToJob job_temp = joblist->FirstJob;
	while (job_temp != NULL)
	{
		if (job_temp->idx == num)
			break;
		job_temp = job_temp->nextJob;
	}
	return job_temp;
}
void add_job(pid_t pid, PtrToCmd cur_cmd, int type, Job_Cond condition)//在后台进程列表中添加进程
{
	PtrToJob new_job = creat_new_job();
	char dst[MAX_CMD_LEN] = { 0 };
	strcpy(dst, cur_cmd->cmd_name);
	PtrToParas para_temp = cur_cmd->cmd_paras->next_para;
	while (para_temp)
	{
		strcat(dst, " ");
		strcat(dst, para_temp->para_value);
		para_temp = para_temp->next_para;
	}
	new_job = set_job(pid, dst, type, condition);
	if (!joblist->FirstJob)
	{
		joblist->FirstJob = new_job;
		joblist->LastJob = new_job;
	}
	else
	{
		new_job->nextJob = joblist->FirstJob;
		joblist->FirstJob->lastJob = new_job;
		joblist->FirstJob = new_job;
	}
	joblist->job_num++;//进程数加一
	new_job->idx = joblist->job_num;//设置新建进程序号
	char cond[MAX_CMD_LEN] = { 0 };
	if (new_job->condition == STOPPED)
		strcpy(cond, "STOPPED");
	else if (new_job->condition == RUNNING)
		strcpy(cond, "RUNNING");
	printf("[%d]\t%d\t%s\t%s\n",new_job->idx, new_job->pid, cond, new_job->jobname);
}
int para_num;       // 位置参数个数
char **para_arg;    // 位置参数列表
// 初始化位置参数
void paramInit(int num, char *arglist[]) {
	para_num = num;
	para_arg = (char **)malloc(para_num * sizeof(char *));
	for (int i = 0; i < para_num; i++) {
		para_arg[i] = (char *)malloc(sizeof(arglist[i]) + 1);
		strcpy(para_arg[i], arglist[i]);
	}
}

// 用户按下 Ctrl+Z 将前台进程挂起
void sig_tstp(int siganal) {
	if (wait_pid !=DEFAULT_PID) {
		kill(wait_pid, SIGSTOP);
	}
}

// 用户按下 Ctrl+\ signal 将前台进程终止
void sig_quit(int signal) {
	if (wait_pid != DEFAULT_PID) {
		kill(wait_pid, SIGQUIT);
	}
}

// 用户按下Ctrl+C signal from keyboard to SIGINT for foreground process
void sig_int(int signal) {
	if (wait_pid != DEFAULT_PID) {
		kill(wait_pid, SIGINT);
	}
}
int fore_wait(pid_t pid)//前台进程等待直到子进程结束或者收到前台继续的信号
{
	wait_pid = pid;
	int status;
	pid_t res_pid = waitpid(pid, &status, WUNTRACED);//暂停当前前台进程，在前台运行pid所指进程，直到子进程发出信号，若子进程处于暂停状态立马返回 
	if (res_pid <= 0)
	{
		printf("Error in wait process\n ");
	}
	wait_pid = DEFAULT_PID;
	return status;//返回子进程运行结束状态
}
int set_run_back(int jobidx)//将后台挂起的进程改为继续运行
{
	PtrToJob job_temp = find_job(jobidx);//在后台列表中寻找该进程
	printf("in run back\n");
	if ( job_temp== NULL)
	{
		printf("Error in set bg: there isn't this job!\n");
		return -1;
	}
	else
	{
		printf("in else back\n");
		job_temp->condition = RUNNING;///将该进程的状态重置
		printf("[%d]\t%d\t%s\tRERUNNING\n", job_temp->idx, job_temp->pid,job_temp->jobname);
		kill(job_temp->pid, SIGCONT);//将该进程继续执行的信号发送
		
	}
	return 0;
}
int set_as_fore(int jobidx)//将后台转到前台
{
	PtrToJob job_temp = find_job(jobidx);//在后台列表中寻找该进程
	if (job_temp == NULL)
	{
		printf("Error in set bg: there isn't this job!\n");
		return -1;
	}
	else
	{   
		job_temp->condition = RUNNING;//将该进程的状态重置,在前台运行
		printf("[%d]\t%d\t%s\tRUNNINGINFORE\n", job_temp->idx, job_temp->pid,job_temp->jobname);
		kill(job_temp->pid, SIGCONT);//将该进程继续执行的信号发送
	}
	printf("error in this line!!\n");
	int temp_status = fore_wait(job_temp->pid);
	if (WIFSTOPPED(temp_status))//如果子进程处于暂停状态
	{
		job_temp->condition = STOPPED;
	}
	return 0;

}


PtrToParas create_para_node()
{
	PtrToParas para_node = (PtrToParas)malloc(sizeof(struct cmd_paras));
	para_node->next_para = NULL;
	para_node->para_value[0] = '\0';//初始化为空字符串
	return para_node;
}
PtrToParas Init_para()
{
	PtrToParas head_para = create_para_node();
	return head_para;
}
PtrToCmd creat_new_cmd()
{
	PtrToCmd new_cmd= (PtrToCmd)malloc(sizeof(struct cmd_attr));
	new_cmd->redirect_file_out = NULL;
	new_cmd->redirect_file_in = NULL;
	new_cmd->bg_flag = 0;
	new_cmd->cmd_paras = NULL;
	new_cmd->cmd_postfix = 0;
	new_cmd->pipe_in_flag = 0;
	new_cmd->pipe_out_flag = 0;
	new_cmd->redirect_int_flag = -1;
	new_cmd->redirect_out_flag = -1;
	new_cmd->test_flag = 0;
	new_cmd->cmd_name = NULL;
	new_cmd->para_num = 0;
	new_cmd->nextcmd = NULL;
	return new_cmd;
}
PtrToCmd Init_cmd()
{
	PtrToCmd new_cmd = creat_new_cmd();
	return new_cmd;
}
void free_paras(PtrToParas para_head)
{
	PtrToParas next = NULL;
	while (para_head)
	{
		next =para_head->next_para;
		free(para_head);
		para_head = next;
	}
}void get_status() {
	int status;
	PtrToJob job_temp = joblist->FirstJob;
	while(job_temp)
	{
		if (job_temp->condition == FINISH)
		{
			job_temp = job_temp->nextJob;
			continue;
		}
		int r = waitpid(job_temp->pid, &status, WNOHANG);
		if (r != 0) 
		{
			if (WIFEXITED(status) | WIFSIGNALED(status)) 
			{       
				fputs("Exit:", stdout);
				char cond[MAX_CMD_LEN] = { 0 };
				if (job_temp->condition == STOPPED)
					strcpy(cond, "STOPPED");
				else if (job_temp->condition == RUNNING)
					strcpy(cond, "RUNNING");
				else if (job_temp->condition == FINISH)
					strcpy(cond, "FINISHED");
				printf("[%d]	%d	%s	%s\n", job_temp->idx, job_temp->pid, cond, job_temp->jobname);
				unset_job(job_temp->pid);
			}
		}
		job_temp = job_temp->nextJob;
	}
}

PtrToCmdLine read_cmd(char* line)
{
	PtrToCmdLine cline = (PtrToCmdLine)malloc(sizeof(struct cmd_line));
	cline->cmd_cnt = 0;
	char* temp_line = (char*)malloc(sizeof(char)*(strlen(line) + 1));
	strcpy(temp_line, line);
	char* cmd_seqs[MAX_CMD_NUM];//存储分割后的每个小命令字符串
	int i;
	for (i = 0; i < MAX_CMD_NUM; i++)
		cmd_seqs[i] = NULL;
	int cmd_cnt = 0;//该行一共可以分为几个子命令
	memset(cline->cmd_line, 0, sizeof(PtrToCmd)*MAX_CMD_NUM);
	if (temp_line)//有效命令
	{
		char delim[] = " \t\n\0";//将换行制表空格NULL作为分隔符
		//对从文件中读入的一行命令进行分割
		int i = 0;
		while (cmd_seqs[i++] = strsep(&temp_line, delim));
		int j = 0;
		int pipe_flag = 0;//该命令是否为管道
		int comman_cmd = 1;//1->comman cmd，判断当前片段是否为普通的命令头，否则为特殊符号或参数
		PtrToCmd cur_cmd = NULL;
		PtrToParas para_head = NULL;//参数链表头
		PtrToParas para_temp = NULL;
		PtrToParas para_pre = NULL;
		for (j = 0; cmd_seqs[j] != NULL; j++)
		{
			if (strlen(cmd_seqs[j]) == 0)//strsep会把连续的属于delim的字符返回为空字符串"，跳过它们
				continue;
			
			if (comman_cmd)
			{
				cur_cmd = creat_new_cmd();
				int cmd_len = 1 + strlen(cmd_seqs[j]);//当前命令的长度
				cur_cmd->cmd_name= (char*)malloc(sizeof(char)*cmd_len);//为当前节点结构的节点名称指针申请空间
				strcpy(cur_cmd->cmd_name, cmd_seqs[j]);//赋值
				para_head = Init_para();//初始化当前命令的参数链表，返回dummyhead
				para_pre = para_head;
				cur_cmd->cmd_paras = para_head;//参数链表赋给当前命令
				comman_cmd = 0;
				if (pipe_flag == 1)//该命令为管道命令，管道输入
				{
					cur_cmd->pipe_in_flag = 1;
					pipe_flag = 0;//恢复pipe标志
				}
				cline->cmd_line[cline->cmd_cnt] = cur_cmd;//该行的命令数加一
				cline->cmd_cnt++;
			}
			else
			{
				if (*(cmd_seqs[j]) == '&')//后台
				{
					cur_cmd->bg_flag = 1;//该命令为后台命令
					comman_cmd = 1;//之后若还有命令一定是新的命令
					continue;
				}
				else if (*(cmd_seqs[j]) == '|')//管道-->输出到管道
				{
					cur_cmd->pipe_out_flag = 1;
					pipe_flag = 1;//下一个命令片段pipe_in为true-->从管道读取
					comman_cmd = 1;//后面是一个新的命令
					continue;
				}
				else if (*(cmd_seqs[j]) == '<' && cmd_seqs[j][1] != '<')//覆盖型重定向
				{
					cur_cmd->redirect_int_flag = 0;//0-cover
					cur_cmd->redirect_file_in = (char*)malloc(sizeof(char) * strlen(cmd_seqs[j + 1]) + 1);
					strcpy(cur_cmd->redirect_file_in, cmd_seqs[j + 1]);//重定向输入文件为下一个cmd片段
					j++;
					continue;
				}
				else if (cmd_seqs[j][0] == '<' && cmd_seqs[j][1] == '<')//添加型重定向输入
				{
					cur_cmd->redirect_int_flag = 1;
					cur_cmd->redirect_file_in = (char*)malloc(sizeof(char) * strlen(cmd_seqs[j + 1]) + 1);
					strcpy(cur_cmd->redirect_file_in, cmd_seqs[j + 1]);//重定向输入文件为下一个cmd片段
					j++;
					continue;
				}
				else if (*(cmd_seqs[j]) == '>'&&cmd_seqs[j][1] != '>')//覆盖型重定向输出
				{
					cur_cmd->redirect_out_flag = 0;
					cur_cmd->redirect_file_out = (char*)malloc(sizeof(char) * strlen(cmd_seqs[j + 1]) + 1);
					strcpy(cur_cmd->redirect_file_out, cmd_seqs[j + 1]);//重定向输出文件为下一个cmd片段
					j++;
					continue;
				}
				else if (cmd_seqs[j][0] == '>'&&cmd_seqs[j][1] == '>')//添加型重定向输出
				{
					cur_cmd->redirect_out_flag = 1;
					cur_cmd->redirect_file_out = (char*)malloc(sizeof(char) * strlen(cmd_seqs[j + 1]) + 1);
					strcpy(cur_cmd->redirect_file_out, cmd_seqs[j + 1]);//重定向输出文件为下一个cmd片段
					j++;
					continue;
				}
				else if (cmd_seqs[j][0] == '-')//test命令
				{
					cur_cmd->cmd_postfix = (char*)malloc(sizeof(char) * (strlen(cmd_seqs[j])+1));//
					strcpy(cur_cmd->cmd_postfix, cmd_seqs[j]);
					cur_cmd->test_flag = 1;
				}
				else if ((cmd_seqs[j][1] == '=')&&(cmd_seqs[j][0] == '='|| cmd_seqs[j][0] == '!'))//test命令==/!=
				{
					cur_cmd->cmd_postfix = (char*)malloc(sizeof(char) * strlen(cmd_seqs[j])+1);
					strcpy(cur_cmd->cmd_postfix, cmd_seqs[j]);
					cur_cmd->test_flag = 1;
				}
				else//添加参数
				{
					para_temp = create_para_node();//创建新参数节点
					para_pre->next_para = para_temp;//将当前新的节点赋给上一节点作为next
					para_pre = para_temp;//更新上一节点
					strcpy(para_temp->para_value, cmd_seqs[j]);//
					cur_cmd->para_num++;//当前命令参数加一
				}
			}
		}
	}
	return cline;
}
int judge_internal(PtrToCmd cmd_cur)
{
	char* cmdname = cmd_cur->cmd_name;
	if (strcmp("cd", cmdname) == 0)//正常
		return 1;
	else if (strcmp("pwd", cmdname) == 0)//正常
		return 2;
	else if (strcmp("clr", cmdname) == 0)//正常
		return 3;
	else if (strcmp("dir", cmdname) == 0)//正常
		return 4;
	else if (strcmp("environ", cmdname) == 0)//正常
		return 5;
	else if (strcmp("echo", cmdname) == 0)
		return 6;
	else if (strcmp("help", cmdname) == 0)
		return 7;
	else if (strcmp("quit", cmdname) == 0)
		return 8;
	else if (strcmp("time", cmdname) == 0)//正常
		return 9;
	else if (strcmp("umask", cmdname) == 0)
		return 10;
	else if (strcmp("jobs", cmdname) == 0)
		return 11;
	else if (strcmp("bg", cmdname) == 0)
		return 12;
	else if (strcmp("fg", cmdname) == 0)
		return 13;
	else if (strcmp("exec", cmdname) == 0)
		return 0;
	else if (strcmp("set", cmdname) == 0)//可用
		return 15;
	else if (strcmp("unset", cmdname) == 0)//可用
		return 16;
	else if (strcmp("shift", cmdname) == 0)
		return 17;
	else if (strcmp("test", cmdname) == 0)
		return 18;
	else if (strcmp("exit", cmdname) == 0)
		return 19;
	else if (strcmp("sleep", cmdname) == 0)
		return 20;
	else if (strcmp("ls", cmdname) == 0)
		return 21;
	else
		return UNINTER;

}	
/****************************内建命令**********************************/
int bg_ins(PtrToCmd cur_cmd)
{
	if (cur_cmd->cmd_paras->next_para == NULL)//用户未传入参数,则对最后一个进程进行操作
	{
		if (joblist->FirstJob == NULL)
		{
			printf("Error in bg: No jobs in background!\n");
			return -1;
		}
		printf("in bg1\n");
		set_run_back(joblist->FirstJob->idx);
	}
	else
	{
		PtrToParas para_temp = cur_cmd->cmd_paras->next_para;
		while (para_temp)
		{
			int index = atoi(para_temp->para_value);
			printf("in bg2\n");
			set_run_back(index);
			para_temp = para_temp->next_para;
		}
		return INS_DONE;
	}
}
int fg_ins(PtrToCmd cur_cmd)
{
	if (cur_cmd->cmd_paras->next_para == NULL)//用户未传入参数,则对最后一个进程进行操作
	{
		if (joblist->FirstJob == NULL)
		{
			printf("Error in bg: No jobs in background!\n");
			return -1;
		}
		set_as_fore(joblist->FirstJob->idx);
	}
	else
	{
		PtrToParas para_temp = cur_cmd->cmd_paras->next_para;
		while (para_temp)
		{
			int index = atoi(para_temp->para_value);
			set_as_fore(index);
			para_temp = para_temp->next_para;
		}
		return INS_DONE;
	}

}
int jobs_ins()
{
	int i;
	PtrToJob temp = joblist->LastJob;
	for (i = 0; i < joblist->job_num; i++)
	{
		if (temp)
		{
			char cond[MAX_CMD_LEN] = { 0 };
			if (temp->condition == STOPPED)
				strcpy(cond, "STOPPED");
			else if (temp->condition == RUNNING)
				strcpy(cond, "RUNNING");
			else if(temp->condition == FINISH)
				strcpy(cond, "FINISH");
			printf("[%d]\t%d\t%s\t%s\n", temp->idx, temp->pid, cond, temp->jobname);
		}
		temp = temp->lastJob;
	}
}

int help_ins(PtrToCmd cur_cmd) 
{
	size_t argc = cur_cmd->para_num;
	PtrToParas para_temp = cur_cmd->cmd_paras->next_para;
	char path[MAX_PATH_LEN];
	char line[MAX_CMD_LEN];
	int ret = 0;
	do {
		char* argv = para_temp->para_value;
		sprintf(path, "./doc/%s", *argv);
		FILE *fp = fopen(path, "r");
		if (fp == NULL) {
			printf(path, "没有%s命令的帮助\n", *argv);
			ret = -1;
			continue;
		}
		while (fgets(line, MAX_CMD_LEN, fp) != NULL) {
			fputs(line, stdout);
		}
	} while (para_temp!=NULL);
	return ret;
}



/***dir ：    内建指令：列出目录<dst_file>的内容****/
int dir_ins(PtrToCmd cur_cmd)//
{
	DIR *dir = NULL;//目录指针
	char* dst_file;
	struct dirent *filename = NULL;//指针，指向目录结构，声明在<dirent.h>
	if (cur_cmd->cmd_paras->next_para == NULL)//没有参数打印当前目录
	{
		char path[MAX_PATH_LEN];
		getcwd(path, MAX_PATH_LEN);
		dst_file = path;
	}
	else
		dst_file = cur_cmd->cmd_paras->next_para->para_value;
	dir = opendir(dst_file);//打开相应的路径
	int cnt = 0;
	if (dir != NULL) 
	{
		while(filename = readdir(dir))
		{
			if (filename == NULL)//已经没有文件可供输出
				break;
			else if (filename->d_name[0] == '.' || filename->d_name[0] == '..')//忽略隐藏目录
				continue;
			else 
				printf("%-16.16s\t", filename->d_name);
			cnt++;
			if (cnt % 6 == 0)
				printf("\n");
		}
		if(cnt %6 != 0)
			printf("\n");
		//closedir(dir);//关闭打开的目录
	}
	else {
		printf("Error:in dir, can't open directory file!\n");//出错信息
		return -1;
	}
	return INS_DONE;
}


int exit_ins()
{
	exit(0);
	return EXIT;
}

int shift_ins(PtrToCmd cur_cmd)//命令行参数移位
{
	int shft_byte = 1;
	if (cur_cmd->cmd_paras->next_para != NULL)
		shft_byte = atoi(cur_cmd->cmd_paras->next_para->para_value);
	for (int i = 0; i < shft_byte; i++)
	{
		free(para_arg[i]);
		para_arg++;
	}
	para_num -= shft_byte;
	return INS_DONE;
}
int env_ins()
{
	int cnt = 40;
	for (size_t i = 0; environ[i] != NULL; i++) {
		fprintf(stdout, "%s\n", environ[i]);
	}
	return INS_DONE;
}
int sleep_ins(PtrToCmd cur_cmd)
{
	int c = atoi(cur_cmd->cmd_paras->next_para->para_value);
	sleep(c);
	return INS_DONE;
}
int set_ins(PtrToCmd cur_cmd)
{
	if (cur_cmd->cmd_paras->next_para == NULL)
		return env_ins();
	setenv(cur_cmd->cmd_paras->next_para->para_value, cur_cmd->cmd_paras->next_para->next_para->para_value, 1);
	return INS_DONE;
}
int unset_ins(PtrToCmd cur_cmd)
{
	int res = INS_DONE;
	PtrToParas para_temp = cur_cmd->cmd_paras->next_para;
	while (para_temp)
	{
		if (unsetenv(para_temp->para_value) == -1)
			res = -1;
		para_temp = para_temp->next_para;
	}
	return res;
}
int echo_ins(PtrToCmd cur_cmd)//未对“”做处理
{
	
	PtrToParas paralist = cur_cmd->cmd_paras->next_para;
	if (cur_cmd->redirect_file_in != NULL)
	{
		char line[MAX_CMD_LEN];
		while ((fgets(line, 100, stdin))!=NULL)
		{
			fputs(line, stdout);
		}
	}
	else
	{
		while (paralist)//参数列表，即为要打印的字符串数组
		{
			char* test_temp = strchr(paralist->para_value, '$');
			//puts(test_temp);
			if (test_temp != NULL)//需要替换变量
			{
				int i = atoi(test_temp);
				test_temp++;
				if((*test_temp<'0')||(*test_temp>'9'))
				{
					char env_print[MAX_CMD_LEN] = { 0 };
					char* env_get = getenv(test_temp);
					if (env_get == NULL)
						printf("No such envirable name!\n");
					else
						puts(env_get);
				}
				else if (i >= 0 && i < para_num)
				{
					printf("%s ", para_arg[i]);
				}
			}
			else //直接打印该句话即可
			{
				printf("%s \n", paralist->para_value);
			}
			paralist = paralist->next_para;
		}
		//printf("\n");
	}
	fflush(stdout);
	return INS_DONE;
}
/****pwd 内建命令：打印当前工作路径****/
int pwd_ins()
{
	char buf[MAXBUFSIZE];
	if (getcwd(buf, sizeof(buf)))//Get current working directory and store it in buffer.
		printf("%s\n", buf);
	else
		printf("Error in pwd: can't getcwd\n");
	return INS_DONE;
}
/***cd 内建命令: 改变当前工作目录***/
int cd_ins(PtrToCmd cur_cmd)
{
	if (cur_cmd->cmd_paras->next_para)
	{
		char* dest = cur_cmd->cmd_paras->next_para->para_value;
		if (chdir(dest) != 0)
		{
			printf("Error in cd: no such directory: %s\n", dest);
			return -1;
		}
		else return INS_DONE;
	}
	else
		return pwd_ins();
}
/**quit  内建命令：退出shell**/
int quit_ins()
{
	return exit_ins();
}
/**exec  内建命令：在进程中启动另一个程序执行**/

int exec_ins(PtrToCmd cur_cmd)//不要有-后缀
{
	char argv[MAX_CMD_NUM][MAX_CMD_LEN] = { 0 };
	char* arg_in[MAX_CMD_NUM] = { 0 };
	PtrToParas para_temp = cur_cmd->cmd_paras->next_para;//参数列表头
	int i = 0;
	while (para_temp)
	{
		strcpy(argv[i], para_temp->para_value);//字符串复制
		arg_in[i] = argv[i];
		para_temp = para_temp->next_para;
		i++;
	}
	arg_in[i] = (char*)0;
	execvp(arg_in[0], arg_in);
	return INS_DONE;
}
/* umask 内建命令：显示修改掩码*/
/***此函数主要使用<sys/stat.h>库中的umask函数对umask进行设置和获取***/
int umask_ins(PtrToCmd cur_cmd)
{
	mode_t mask;
	if (cur_cmd->cmd_paras->next_para != NULL)//用户重置mask
		mask = (mode_t)strtol(cur_cmd->cmd_paras->next_para->para_value, NULL, 8);//将以字符串形式存储的参数转换为int型
	
	else//默认设置；0022
		mask = umask(S_IWOTH | S_IWGRP);//屏蔽其他和群组的写权限
	umask(mask);
	printf("%04o\n", mask);//打印umask值
	return INS_DONE;
}
/**clear 内建命令：清屏**/

int clear_ins()
{
	printf("\033[2J\033[0;0H");//\033[2J->清屏 \033[0;0H->光标置顶
	return INS_DONE;
}
/***time 内建命令：获取当前时间***/
int time_ins()//获取时间指令
{
	time_t timep;
	time(&timep);
	printf("%s", asctime(gmtime(&timep)));
	return INS_DONE;
}


/**test 内建命令：数值文件字符串检查**/
int test_ins(PtrToCmd cur_cmd) 
{
	/*************************数值比较****************************************/
	/*****此部分使用atoi函数将参数列表中存储的字符串类型的参数转换为整形*****/
	if (strcmp(cur_cmd->cmd_postfix, "-eq") == 0)//判断两参数是否相等
	{
		int a = atoi(cur_cmd->cmd_paras->next_para->para_value);
		int b = atoi(cur_cmd->cmd_paras->next_para->next_para->para_value);
		return a == b;
	}
	else if (strcmp(cur_cmd->cmd_postfix, "-ne") == 0)//判断两参数是否不等
	{
		int a = atoi(cur_cmd->cmd_paras->next_para->para_value);
		int b = atoi(cur_cmd->cmd_paras->next_para->next_para->para_value);
		return  a != b;
	}
	else if (strcmp(cur_cmd->cmd_postfix, "-ge") == 0)//>=
	{
		int a = atoi(cur_cmd->cmd_paras->next_para->para_value);
		int b = atoi(cur_cmd->cmd_paras->next_para->next_para->para_value);
		return a >= b;
	}
	else if (strcmp(cur_cmd->cmd_postfix, "-gt") == 0)//>
	{
		int a = atoi(cur_cmd->cmd_paras->next_para->para_value);
		int b = atoi(cur_cmd->cmd_paras->next_para->next_para->para_value);
		return a > b;
	}
	else if (strcmp(cur_cmd->cmd_postfix, "-le") == 0)//<=
	{
		int a = atoi(cur_cmd->cmd_paras->next_para->para_value);
		int b = atoi(cur_cmd->cmd_paras->next_para->next_para->para_value);
		return a <= b;
	}
	else if (strcmp(cur_cmd->cmd_postfix, "-lt") == 0)//<
	{
		int a = atoi(cur_cmd->cmd_paras->next_para->para_value);
		int b = atoi(cur_cmd->cmd_paras->next_para->next_para->para_value);
		return a < b;
	}
	/*****************字符串判断*********************************/
	else if (strcmp(cur_cmd->cmd_postfix, "-n") == 0)//判断字符串是否为空
	{
		return strlen(cur_cmd->cmd_paras->next_para->para_value)!=0;
	}
	else if (strcmp(cur_cmd->cmd_postfix, "-z") == 0)//判断字符串是否非空
	{
		return strlen(cur_cmd->cmd_paras->next_para->para_value) == 0;
	}
	else if (strcmp(cur_cmd->cmd_postfix, "==") == 0)//字符串是否相等
	{
		return !strcmp(cur_cmd->cmd_paras->next_para->para_value, cur_cmd->cmd_paras->next_para->next_para->para_value);
	}
	else if (strcmp(cur_cmd->cmd_postfix, "!=") == 0)//字符串是否不等
		return strcmp(cur_cmd->cmd_paras->next_para->para_value, cur_cmd->cmd_paras->next_para->next_para->para_value);
	/***************************文件判断*********************************/
	/******此部分使用access函数对参数列表中存储的文件进行类型判断*********/
	else if (strcmp(cur_cmd->cmd_postfix, "-e") == 0)
	{
		return (access(cur_cmd->cmd_paras->next_para->para_value, F_OK) == 0);
	}
	else if (strcmp(cur_cmd->cmd_postfix, "-r") == 0)
	{
		return (access(cur_cmd->cmd_paras->next_para->para_value, R_OK) == 0);
	}
	else if (strcmp(cur_cmd->cmd_postfix, "-w") == 0)
	{ 
		return (access(cur_cmd->cmd_paras->next_para->para_value, W_OK) == 0);
	}
	else if (strcmp(cur_cmd->cmd_postfix, "-x") == 0)
	{ 
		return (access(cur_cmd->cmd_paras->next_para->para_value, X_OK) == 0);
	}
	else return -1;
}

void print_r_li(struct stat st, int object)//判断对象读权限
{
	int res;
	res = st.st_mode& object;
	if (res == object)
		printf("r");
	else
		printf("-");
}
void print_w_li(struct stat st, int object)//判断对象写权限
{
	int res;
	res = st.st_mode& object;
	if (res == object)
		printf("w");
	else
		printf("-");
}
void print_x_li(struct stat st, int object)//判断对象执行权限
{
	int res;
	res = st.st_mode& object;
	if (res == object)
		printf("x");
	else
		printf("-");
}
void print_file_type(struct stat st)
{
	int res = st.st_mode & S_IFMT;
	if ( res== S_IFREG)
		printf("-");
	else if (res == S_IFDIR)
		printf("d");
	else if (res == S_IFIFO)
		printf("p");
}

int ls_ins(PtrToCmd cur_cmd)
{
	if (cur_cmd->cmd_postfix == NULL)
		return dir_ins(cur_cmd);
	DIR *dir = NULL;//目录指针
	char* dst_file;
	struct dirent *filename = NULL;//指针，指向目录结构，声明在<dirent.h>
	struct stat st;
	if (cur_cmd->cmd_paras->next_para == NULL)//没有参数打印当前目录
	{
		char path[MAX_PATH_LEN];
		getcwd(path, MAX_PATH_LEN);
		dst_file = path;
	}
	else
		dst_file = cur_cmd->cmd_paras->next_para->para_value;
	dir = opendir(dst_file);//打开相应的路径
	int cnt = 0;
	if (dir != NULL)
	{
		while (filename = readdir(dir))
		{
			if (filename == NULL)//已经没有文件可供输出
				break;
			else if (filename->d_name[0] == '.' || filename->d_name[0] == '..')//忽略隐藏目录
				continue;
			else
			{
				struct group* grp = getgrgid(st.st_gid);
				struct passwd*  pass = getpwuid(st.st_uid);
				stat(filename->d_name, &st);
				//printf("%-16.16s\t", filename->d_name);
				print_file_type(st);//判断文件类型
				print_r_li(st, S_IRUSR);//读权限，用户
				print_w_li(st, S_IWUSR);//写权限，用户
				print_x_li(st, S_IXUSR);//执行权限，用户
				print_r_li(st, S_IRGRP);//读权限，群组
				print_w_li(st, S_IWGRP);//写权限，群组
				print_x_li(st, S_IXGRP);//执行权限，群组
				print_r_li(st, S_IROTH);//读权限，其他
				print_w_li(st, S_IWOTH);//写权限，其他
				print_x_li(st, S_IXOTH);	//执行权限，其他	
				printf("\t%3d\t ", (long)st.st_nlink);//链接数
				printf("%-10s %-10s ", pass->pw_name, grp->gr_name);//用户名称，群组名称
				printf("\t%8d\t ", (long long)st.st_size);//文件大小
				//printf("\t%s", ctime(&st.st_atime)); //最近一次访问时间
				//printf("%-25s ", filename->d_name);//文件名称
				
			}
		}
	}
	else {
		printf("Error:in dir, can't open directory file!\n");//出错信息
		return -1;
	}
	return INS_DONE;
}
		

int exe_internal(PtrToCmd cmd_cur)
{
	char* cmdname = cmd_cur->cmd_name;
	if (strcmp("cd", cmdname) == 0)//1
		return cd_ins(cmd_cur);
	else if (strcmp("pwd", cmdname) == 0)//1
		return pwd_ins();
	else if (strcmp("clr", cmdname) == 0)//1
		return clear_ins();
	else if (strcmp("dir", cmdname) == 0)//1
		return dir_ins(cmd_cur);
	else if (strcmp("environ", cmdname) == 0)//1
		return env_ins();
	else if (strcmp("echo", cmdname) == 0)//1
		return echo_ins(cmd_cur);
	else if (strcmp("help", cmdname) == 0)
		return help_ins(cmd_cur);
	else if (strcmp("quit", cmdname) == 0)//1
		return quit_ins();
	else if (strcmp("time", cmdname) == 0)//1
		return time_ins();
	else if (strcmp("umask", cmdname) == 0)//1
		return umask_ins(cmd_cur);
	else if (strcmp("jobs", cmdname) == 0)//1
		return jobs_ins();
	else if (strcmp("bg", cmdname) == 0)//1
		return bg_ins(cmd_cur);
	else if (strcmp("fg", cmdname) == 0)//1
		return fg_ins(cmd_cur);
	else if (strcmp("exec", cmdname) == 0)//1
		return exec_ins(cmd_cur);
	else if (strcmp("set", cmdname) == 0)//1
		return set_ins(cmd_cur);
	else if (strcmp("unset", cmdname) == 0)//1
		return unset_ins(cmd_cur);
	else if (strcmp("shift", cmdname) == 0)//1
		return shift_ins(cmd_cur);
	else if (strcmp("test", cmdname) == 0)//1
	{
		int flag = test_ins(cmd_cur);
		if (flag == 1)
			printf("TRUE\n");
		else
			printf("FALUSE\n");
		return test_ins(cmd_cur);
	}
		
	else if (strcmp("exit", cmdname) == 0)//1
		return exit_ins();
	else if (strcmp("sleep", cmdname) == 0)//1
		return sleep_ins(cmd_cur);
	else if (strcmp("ls", cmdname) == 0)//1
		return ls_ins(cmd_cur);
	else
		return UNINTER;

}
void exe_cmd(PtrToCmd cur_cmd)//开始执行每个命令
{

	get_status();
	int re_fd1 = 0;
	int re_fd2 = 0;
	int in_fd = dup(fileno(stdin));
	int out_fd = dup(fileno(stdout));
	if (cur_cmd->redirect_int_flag >= 0)//
	{

		if (cur_cmd->redirect_int_flag == 0)
		{
			if (access(cur_cmd->redirect_file_in, F_OK) == 0)//文件存在
				re_fd1 = open(cur_cmd->redirect_file_in, O_RDONLY | O_CREAT );//文件默认权限为0644
			else
			{
				printf("Error in redirect: input file not exist!\n");
				return;
			}
		}
		else
		{
			if (access(cur_cmd->redirect_file_in, F_OK) == 0)
				re_fd1 = open(cur_cmd->redirect_file_in, O_APPEND | O_CREAT );
			else
			{
				printf("Error in redirect: input file not exist!\n");
				return;
			}
		}
		close(0);//关闭标准输入
		if (dup2(re_fd1, 0) < 0)
		{
			printf("Error in redirect in!\n");
			exit(1);
		}
		
	}
	if (cur_cmd->redirect_out_flag >= 0)
	{
		if (cur_cmd->redirect_out_flag == 0)
		{
			if (access(cur_cmd->redirect_file_out, F_OK) == 0)
				re_fd2 = open(cur_cmd->redirect_file_out, O_RDWR | O_CREAT | O_TRUNC);
			else
				re_fd2 = open(cur_cmd->redirect_file_out, O_RDWR | O_CREAT | O_TRUNC, 0644);
		}
		else
		{
			if (access(cur_cmd->redirect_file_out, F_OK) == 0)
				re_fd2 = open(cur_cmd->redirect_file_out, O_APPEND | O_CREAT | O_RDWR);
			else
				re_fd2 = open(cur_cmd->redirect_file_out, O_APPEND | O_CREAT | O_RDWR, 0644);
		}
		close(1);//关闭标准输入
		if (dup2(re_fd2, 1) < 0)
		{
			printf("Error in redirect out!\n");
			exit(1);
		}
		
	}
	if(re_fd1)
		close(re_fd1);//关闭创建的文件描述符
	if(re_fd2)
		close(re_fd2);//关闭创建的文件描述符	
	//处理重定向操作
	int internal_flag = 0;
	if (judge_internal(cur_cmd) > 0)
		internal_flag = 1;
	if ((internal_flag&&(cur_cmd->bg_flag==0))&&(cur_cmd->pipe_out_flag!=1))//前台内建命令,直接运行该命令
		exe_internal(cur_cmd);
	else
	{
		if (cur_cmd->pipe_out_flag)
			pipe(pipeFd);
		pid_t pid, q;
		q = getpid();
		pid = fork(); //创建子进程
		
		if (pid < 0)
		{
			printf("Error: Failed in fork!\n");
			exit(1);
		}
		else if (pid == 0)//子进程
		{
			if (cur_cmd->pipe_out_flag)
			{
				close(fileno(stdout));//关闭标准输出
				close(pipeFd[0]);//关闭管道读
				dup2(pipeFd[1], fileno(stdout)); //重定向输出，将结果写入管道
				close(pipeFd[1]);//关闭管道写
			}
     		if (internal_flag == 1)//子进程，内建命令,后台
				exe_internal(cur_cmd);
			else
			{
				if (strcmp(cur_cmd->cmd_name, "exec")==0)
					exec_ins(cur_cmd);
				else
				{
					char argv[MAX_CMD_NUM][MAX_CMD_LEN] = { 0 };
					char* arg_in[MAX_CMD_NUM] = { 0 };
					PtrToParas para_temp = cur_cmd->cmd_paras->next_para;//参数列表头
					strcpy(argv[0], cur_cmd->cmd_name);
					arg_in[0] = argv[0];
					int i = 1;
					while (para_temp)
					{
						strcpy(argv[i], para_temp->para_value);//字符串复制
						arg_in[i] = argv[i];
						para_temp = para_temp->next_para;
						i++;
					}
					arg_in[i] = (char*)0;
					if (cur_cmd->bg_flag == 1)//子进程，后台进行外部命令
					{
						execvp(cur_cmd->cmd_name, arg_in);
					}
					else//子进程，前台进行外部命令
					{
						puts(arg_in[0]);
						if (execvp(arg_in[0], arg_in) < 0)
						{
							printf("Error in getting outer command\n");
							exit(-1);
						}
					}
				}
			}
			exit(0);

		}
		else //父进程
		{
			if (cur_cmd->pipe_out_flag == 1)//
			{
				int pipeChildPID = 0;
				int i = 0;
				//printf("get here3!\n");
				if ((pipeChildPID = fork()) < 0)
				{
					perror("Create child process Failure!");
				}
				else if (pipeChildPID == 0)//子进程，接收管道数据
				{
					//printf("gethere3!\n");
					close(pipeFd[1]);//关闭管道写
					close(fileno(stdin));
					dup2(pipeFd[0], fileno(stdin));//将原本与标准输入相对应的接口转接到管道的出口上
					char buf_r[100] = { 0 };
					read(pipeFd[0], buf_r, 100);
					close(pipeFd[0]);
					dup2(out_fd, fileno(stdout));
					dup2(in_fd, fileno(stdin));
					//printf("here!!!!\n");
					char argv[MAX_CMD_NUM][MAX_CMD_LEN] = { 0 };
					char* arg_in[MAX_CMD_NUM] = { 0 };
					//printf("here!\n");
					//puts(cur_cmd->nextcmd->cmd_name);
					PtrToParas para_temp = cur_cmd->nextcmd->cmd_paras->next_para;//参数列表头
					strcpy(argv[0], cur_cmd->nextcmd->cmd_name);
					arg_in[0] = argv[0];
					int i = 1;
					//printf("get here1!\n");
					while (para_temp)
					{
						strcpy(argv[i], para_temp->para_value);//字符串复制
						arg_in[i] = argv[i];
						para_temp = para_temp->next_para;
						i++;
					}
					arg_in[i] = buf_r;
					printf("%d\n", strlen(buf_r));
					buf_r[strlen(buf_r)-1] = '\0';
					buf_r[strlen(buf_r) - 1] = '\0';
					printf("%d\n", strlen(buf_r));
					//puts(buf_r);
					puts(arg_in[0]);
					puts(arg_in[1]);
					arg_in[++i] = (char*)0;
					if (execvp(arg_in[0], arg_in) < 0)
					{
						printf("Error in getting outer command\n");
						exit(-1);
					}
					exit(0);
				}
				else
				{
					int status;
					waitpid(pipeChildPID, &status, 0);//等待管道信息接收并执行完毕
				}
			}
			if (cur_cmd->bg_flag==0)//如果此命令是一个前台进程，父进程需要等待直到收到信号(子进程结束)
			{
				int back_status = fore_wait(pid);//父进程等待pid为pid的子进程
				if (WIFSTOPPED(back_status))//如果是挂起的子进程，需要将子进程添加进后台列表中，其他的因为已经完成，无需添加
					add_job(pid, cur_cmd, 1, STOPPED);
			}
			else
				add_job(pid, cur_cmd, 1, STOPPED);
		}
	}
	if (cur_cmd->redirect_int_flag >= 0)
	{
		if (-1 == dup2(in_fd, fileno(stdin)))
			printf("Error in dup!\n");
		close(in_fd);
	}
	if (cur_cmd->redirect_out_flag >= 0)
	{
		if (-1 == dup2(out_fd, fileno(stdout)))
			printf("Error in dup!\n");
		close(out_fd);
	}
	if (cur_cmd->pipe_in_flag)
	{
		dup2(in_fd, fileno(stdin));
	}
	if (cur_cmd->pipe_out_flag)
	{
		dup2(out_fd, fileno(stdout));
	}
}
void exe_line(PtrToCmdLine clist)
{
	//初始化管道
	
	int k = 0;
	int cnt = 0;
	PtrToCmd cur_cmd = clist->cmd_line[k];
	while ((cur_cmd = clist->cmd_line[k++]))
	{
		if (cur_cmd->pipe_out_flag == 1)//管道命令，将后一命令设为此命令节点的后一个，链接起来
		{
			cur_cmd->nextcmd = creat_new_cmd();
			cur_cmd->nextcmd->cmd_name = (char*)malloc(sizeof(clist->cmd_line[k]->cmd_name) + 1);
			strcpy(cur_cmd->nextcmd->cmd_name, clist->cmd_line[k]->cmd_name);
			PtrToParas para_temp = clist->cmd_line[k]->cmd_paras->next_para;
			cur_cmd->nextcmd->cmd_paras = create_para_node();
			PtrToParas para2 = cur_cmd->nextcmd->cmd_paras->next_para;
			cur_cmd->nextcmd->cmd_paras->next_para = create_para_node();
			if (para_temp->para_value)
			{
					strcpy(cur_cmd->nextcmd->cmd_paras->next_para->para_value, para_temp->para_value);
			}
			if (clist->cmd_line[k]->cmd_postfix)
			{
				cur_cmd->nextcmd->cmd_postfix = (char*)malloc(sizeof(char)*strlen(clist->cmd_line[k]->cmd_postfix + 1));
				strcpy(cur_cmd->nextcmd->cmd_postfix, clist->cmd_line[k]->cmd_postfix);
			}
			k++;
		}

	}
	
	k = 0;
	while ((cur_cmd = clist->cmd_line[k++]))
	{
		if (cur_cmd->pipe_out_flag == 1)
			k++;
		exe_cmd(cur_cmd);
		cnt++;
	}
	for (k = 0; k < cnt; k++)
	{
		free_paras(clist->cmd_line[k]->cmd_paras);
		free(clist->cmd_line[k]);
	}
}
char* get_dir()//获取当前工作目录
{
	char buf[MAXBUFSIZE];
	int cnt = readlink("/proc/self/exe", CUR_ABSO_PATH, 1000);// /proc/self/exe 代表当前程序，用readlink读取可执行文件所在的路径
	if (cnt < 0 || cnt >= MAXBUFSIZE) {//地址过长或者获取失败
		exit(-1);//返回
	}
	getcwd(buf, sizeof(buf));//getcwd函数获取当前工作目录绝对路径
	return buf;
}
void display_prefix()//打印每行命令的前缀。即当前工作目录
{
	char buf[MAXBUFSIZE];
	getcwd(buf, sizeof(buf)); 
	printf("\033[32;1mmaxine_shell\033[37m\033[34m%s# \033[0m", buf);
	fflush(stdout);
}
void setpath(char *newPath) 
{
	setenv("PATH", newPath, 1);
}
int get_cmd(char* cmd_line)
{
	if (fgets(cmd_line, MAX_CMD_LEN, stdin) == NULL) 
		return -1;
	return strlen(cmd_line);
}
void run_outfile(int argc, char**argv)
{
	char this_line[MAX_CMD_LEN];
	int oldInFd = dup(fileno(stdin));//对原来的标准输入文件标识进行备份
	for (int i = 1; i < argc; i++)
	{
		int fd = open(argv[i], O_RDONLY);
		if (fd == -1) {
			printf("Error:cant't open the source file!\n");
			continue;
		}
		dup2(fd, fileno(stdin));//重定向输入为fd所指向文件，即为参数列表中的文件
		while (1)
		{
			if (get_cmd(this_line) == -1)
				break;
			read_cmd(this_line);
			PtrToCmdLine cmdline = read_cmd(this_line);
			exe_line(cmdline);
			fflush(stdout);
		}
		close(fd);
	}
	dup2(oldInFd, STDIN_FILENO);//恢复为标准输入
}
void run_terminal(int argc, char**argv)
{
	char this_line[MAX_CMD_LEN];
	while (1)
	{
		display_prefix();
		if (get_cmd(this_line) == -1)
			break;
		PtrToCmdLine cmdline = read_cmd(this_line);
		exe_line(cmdline);
		fflush(stdout);
	}
}

int main(int argc, char* argv[])
{
	signal(SIGQUIT,sig_quit);
	signal(SIGTSTP, sig_tstp);
	signal(SIGINT,sig_int);
	char buf[MAXBUFSIZE];
	getcwd(buf, sizeof(buf));//getcwd函数获取当前工作目录绝对路径
	setenv("pwd",buf,1);
	setenv("PATH", "/bin",1);//execvp函数调用外部命令需在此目录下寻找命令
	paramInit(argc, argv);
	if (argc > 1)//deal with outfile
		run_outfile(argc, argv);
	else
		run_terminal(argc, argv);
}
