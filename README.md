# Maxine_shell
A simple shell for Linux course of ZJU
**************** ***************************************************************************************
USER_HELP
*******************************************************************************************************
Maxine _shell ： This program  is a simple myshell, including the following functions:
1)Internal commands supported by myshell:
bg, cd, clr, dir, echo, exec, exit, environ, fg, help, jobs, pwd, quit, set, shift, test
time, umask, unset, sleep, ls - - l total 20 instructions
2) Support redirection (<, >>, >), p ipeline (| ), background operation (&)
3) Support all external commands
4) Support signal input Ctrl + Z to suspend the foreground process, Ctrl+C to stop the foreground
process
5) Support shell script file execution
*************************************** ********************************** ******************************
HELP FOR INTERNAL COMMANDS
************************************************************************** *****************************
1.cd:
Function: Change the current working directory
Use for mat: cd filename (change the working directory to filename)
cd returns to the parent directory if no parameters are passed in
2.echo:
Function: Display string
Use format: echo str (display string str)
echo $PATH, echo $1 (support $ print environment  variables and positional parameters)
3.exec:
Function: call and execute commands
Use format: exec date (execute the execution command date in a new process)
4.exit/quit:
Function: Exit the shell
Use format: exit/quit
5.pwd:
Function: Print the absolute pat h of the current directory
Use format: pwd
6.time:
Function: Get the current time
Use format: time
7.umask:
Function: Display the current mask, pass in parameters to reset the mask
Use format:
umask (no parameter passed in to display the current mask)
umask [X] (set a new mask x)
8.shift:
Function: Move the position parameter to the left, one position at a time
Use format: shift
9.test:
Function 1: Compare integer size
Use format:
test int1 [op] int2
op:
- - eq: int1 = int2
- - ne: int1 != int2
- - ge: i nt1 >= int2
- - gt: int1> int2
- - le: int1 <= int2
- - lt: int1 <int2
test [op] string:
op:
- - n: string to determine whether the string is not empty
- - z: string: Determine whether the string is empty
test string1 == string2: Determine whether the st rings are equal
test string1 != string2: Determine whether the strings are not equal
test [op] filename
op:
- - e: filename Determine whether the file exists
- - r: filename Determine whether the file has read permission
- - w: filename Determine whether  the file has write permission
- - x: filename Determine whether the file has execution permission
10.set
Function: Display and set environment variables
Use format:
set does not pass in parameters and displays environment variables
set PATH /home pass i n parameters to set environment variables
11.unset
Function: delete environment variables
Use format: unset
12.jobs
Function: Display the current task list and task status
Use format: jobs
13.fg:
Function: Put the designated background process to the foreg round to continue running.
Use format:
fg (the user did not pass in the process parameters, the last process in the process list is
executed)
fg 1 (put the process with the specified pid or index to the foreground)
14.bg:
Function: put the designated b ackground process in the background.
Use format:
bg (the user did not pass in the process parameters, the last process in the process list is
executed)
bg 1 (put the process with the specified pid or index to the foreground)
15.clr:
Function: put the d esignated background process in the background.
Use format: clr
16.environ:
Function: List all environment variables
Use format: environ
17.help
Function: Display user manual
Use format: help
18.dir:
Function: Display the file composition under the directo ry
Use format:
dir (if the user does not pass in parameters, the file composition in the current directory
will be displayed)
dir filename (display the file composition in the specified directory)
19.ls
Function: Display detailed information of files i n the directory
Use format:
ls (same as dir)
ls - - l (each line displays the user authority, number of links, last access time, etc. of a
file in the current directory)
20. sleep
Function: execute the header file suspended for a period of time
Use fo rmat: sleep 10 (suspend for 10 seconds)
********************************************************************************* **********************
HELP FOR direction and pipeline
********************************************************************************* * **********************
Interpretation of redirection function:
**************************************
The default standard input of the program is input in the command line window
and the default output is standard output, which is the default output to t he command line window.
Redirection changes the default input and output direction of the program,
such as using a file as the input of the program or outputting the output of the program to a file.
After redirection, the input and output of the program are no longer necessarily the command window.
***********
Use format:
***********
command> file1: redirect the output of command to file1, if file1 does not exist, create a
new file file1, if it exists, overwrite file1
command >> file2: redirect the o utput of command to file2, if file2 does not exist, create a
new file file2,
if it exists, add the output result to the end of file2
command <file3: command takes the contents of file3 as input
************************************
Interpretation  of pipeline function:
************************************
The output of the previous instruction is the input of the next instruction.
***********
Use format:
***********
command1 | command2: The output of command1 is passed into command2 as an input para meter
*******************************************************************************************************
HELP FOR Execute external commands
*******************************************************************************************************
Interpr etation of external command function:
*********************************************
The parent process forks a child process and calls an external command in the child process.
The parent process waits for the child process to execute the external command, ,
and the execution result of the external command returns to the shell
***********
Use format:
************
Enter the external command directly, if it exists, it will be called automatically
if it does not exist, it will return to the instruction error pr ompt.
Note that the program automatically searches for external commands from the current environment
variable PATH.
Generally, external commands are stored in /usr/bin.
If you use special external commands, please use the set command to set the environm ent variable PATH
first
*******************************************************************************************************
HELP FOR background execution
************************************************************************************************** *****
Interpretation of background functions:
***************************************
The foreground running command is executed directly. A Shell terminal has only one foreground process.
The execution of the next command can only be carried out after th e execution of the current foreground
command is finished.
The background operation realizes that the foreground reads and executes the command normally while
running other commands in the background.
************
Use format:
************
command &: add'&' after the command, pay attention to the space before and after the'&'
*******************************************************************************************************
HELP FOR execute executable file
************************************************ *******************************************************
Function interpretation:
*************************
Ordinary commands are directly input and executed on the command line.
When executing an executable file, the file is input as a parameter,
and the input of the redirected shell is a specified file, and executed line by line.
*********** ****
Use format:
*********** ****
myshell filename
*******************************************************************************************************
Augst 20 202 0 Yuxuan Cao
