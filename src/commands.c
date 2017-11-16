#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <fcntl.h>

#define UNIX_PATH_MAX 108
#define SOCK_PATH "tpf_unix_sock.server"
#define DATA "Hello from server"

#define SERVER_PATH "tpf_unix_sock.server"
#define CLIENT_PATH "tpf_unix_sock.client"

#include <sys/types.h>
#include <sys/wait.h>

#include "commands.h"
#include "built_in.h"
/*
struct sockaddr_un {
  unsigned short int sun_family;
  char sun_path[UNIX_PATH_MAX];
};
*/

struct thread_data{
  struct single_command* com;
};

struct thread_data t_data;


static struct built_in_command built_in_commands[] = {
  { "cd", do_cd, validate_cd_argv },
  { "pwd", do_pwd, validate_pwd_argv },
  { "fg", do_fg, validate_fg_argv }
};

static int is_built_in_command(const char* command_name)
{
  static const int n_built_in_commands = sizeof(built_in_commands) / sizeof(built_in_commands[0]);

  for (int i = 0; i < n_built_in_commands; ++i) {
    if (strcmp(command_name, built_in_commands[i].command_name) == 0) {
      return i;
    }
  }

  return -1; // Not found
}

void* runner(void* threadarg)
{
  struct thread_data *my_data;
  struct single_command* com;
  
  sleep(1);
  my_data = (struct thread_data *) threadarg;
  com =  my_data->com;

  pid_t c_pid, pid;
  int status;
  c_pid = fork();
  
  remove("text.txt");
  int fd = open("text.txt", O_RDWR|O_CREAT, 0644);

  if(c_pid==0){
    pid = getpid();
    printf("runner print %s", com->argv[0]);
    dup2(fd, STDOUT_FILENO);
    execv(com->argv[0], com->argv);
    fprintf(stderr, "%s:command not found\n", com->argv[0]);
    return;
  }
  else if(c_pid>0){
    pid = wait(&status);
   
  }
  else{
    fprintf(stderr, "Fork Failed\n");
    exit(1);
    return;
  }

  int client_sock, rc, len;
  struct sockaddr_un server_sockaddr;
  struct sockaddr_un client_sockaddr;
  char buf[256];
  memset(&server_sockaddr, 0, sizeof(struct sockaddr_un));
  memset(&client_sockaddr, 0, sizeof(struct sockaddr_un));

  client_sockaddr.sun_family = AF_UNIX;
  strcpy(client_sockaddr.sun_path, CLIENT_PATH);
  len = sizeof(client_sockaddr);
  

  client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if(client_sock == -1){
    printf("SOCKET ERROR\n");
    exit(1);

  }

  unlink(CLIENT_PATH);
  rc = bind(client_sock, (struct sockaddr *) &client_sockaddr, len);
  if(rc == -1){
    printf("CLIENT BIND ERROR: \n");
    close(client_sock);
    exit(1);

    }
  else
  {
    printf("Client Bind SUCCESS\n");
  }

  server_sockaddr.sun_family = AF_UNIX;
  strcpy(server_sockaddr.sun_path, SERVER_PATH);
  rc = connect(client_sock, (struct sockaddr *) &server_sockaddr, len);
  if(rc == -1){
  printf("CONNECT ERROR\n");
  close(client_sock);
  exit(1);
  }
  else
  {
   printf("Client Connect Success\n");
  }
  fd = open("text.txt", O_RDONLY);
  read(fd, buf, sizeof(buf));
  printf("Sending DATA...%s\n", buf);
  rc = send(client_sock, buf, sizeof(buf), 0);
 
  if(rc == -1){
    printf("SEND ERROR\n");
    close(client_sock);
    exit(1);
  }
  else{
    printf("data sent\n");
  }
  close(client_sock);
  pthread_exit(0);
}

/*
 * Description: Currently this function only handles single built_in commands. You should modify this structure to launch process and offer pipeline functionality.
 */
int evaluate_command(int n_commands, struct single_command (*commands)[512])
{
  struct single_command* com = (*commands);
  if (n_commands ==  1) {

    assert(com->argc != 0);

    int built_in_pos = is_built_in_command(com->argv[0]);
    if (built_in_pos != -1) {
      if (built_in_commands[built_in_pos].command_validate(com->argc, com->argv)) {
        if (built_in_commands[built_in_pos].command_do(com->argc, com->argv) != 0) {
          fprintf(stderr, "%s: Error occurs\n", com->argv[0]);
        }
      } else {
        fprintf(stderr, "%s: Invalid arguments\n", com->argv[0]);
        return -1;
      }
    } else if (strcmp(com->argv[0], "") == 0) {
      return 0;
    } else if (strcmp(com->argv[0], "exit") == 0) {
      return 1;
    }
      
      else {
      pid_t c_pid, pid;
      int status;
      c_pid = fork();

      if(c_pid==0){
        pid = getpid();
        execv(com->argv[0], com->argv);
        fprintf(stderr, "%s: command not found\n", com->argv[0]);
	return 1;
      }
      else if(c_pid>0){
        pid = wait(&status);
      }
      else{
        fprintf(stderr, "Fork failed");
        exit(1);
	return -1;
      }
      return 0;
 //     fprintf(stderr, "%s: command not found\n", com->argv[0]);
 //     return -1;
    }
  }
  else if (n_commands == 2) {
      struct single_command* com = *commands + 1;
      pid_t c_pid, pid;
      int status;
      c_pid = fork();

      if(c_pid==0){
        pid = getpid();
        execv(com->argv[0], com->argv);
        fprintf(stderr, "%s: command not found \n", com->argv[0]);
        return 1;
      }
      else if(c_pid > 0){
        pid = wait(&status);
      }
      else {
        fprintf(stderr, "Fork failed");
        exit(1);
        return -1;
      }
      
      int server_sock, client_sock, len, rc;
      int bytes_rec = 0;
      struct sockaddr_un server_sockaddr;
      struct sockaddr_un client_sockaddr;
      char buf[256];
      int backlog = 10;
      memset(&server_sockaddr, 0, sizeof(struct sockaddr_un));
      memset(&client_sockaddr, 0, sizeof(struct sockaddr_un));
      memset(buf, 0, 256);

      server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
      if (server_sock == -1){
        printf("SOCKET ERROR\n");
        exit(1);
      }
      else
      {
        printf("Server program Server socket making success\n");
      }

      server_sockaddr.sun_family = AF_UNIX;
      strcpy(server_sockaddr.sun_path, SOCK_PATH);
      len = sizeof(server_sockaddr);

      unlink(SOCK_PATH);
      rc = bind(server_sock, (struct sockaddr *) &server_sockaddr, len);
      if(rc == -1){
        printf("SERVER BIND ERROR:\n");
        close(server_sock);
        exit(1);
      }
      else
      {
       printf("Server program Bind Success\n");
      }
      
      pthread_t tid;
      int temp =0;
      //Fix needed
      t_data.com = *commands;
      pthread_create(&tid, NULL, runner, &t_data);
   
      rc = listen(server_sock, backlog);
      if(rc == -1){
        printf("LISTEN ERROR:\n");
        close(server_sock);
        exit(1);
      }
     
      printf("server is listening...\n");

      pthread_join(tid,NULL);

      client_sock = accept(server_sock, (struct sockaddr *) &client_sockaddr, &len);
      if(client_sock == -1){
        printf("ACCEPT ERROR:\n");
        close(server_sock);
        close(client_sock);
        exit(1);
      }
      else
      {
        printf("server program accept success\n");
      }

      len = sizeof(client_sockaddr);
      rc = getpeername(client_sock, (struct sockaddr *) &client_sockaddr, &len);
      if(rc == -1){
        printf("GETPEERNAME ERROR:\n");
        close(server_sock);
        close(client_sock);
        exit(1);
      }
      else {
        printf("Client socket filepath: %s\n", client_sockaddr.sun_path);
      }

      memset(buf, 0, 256);
      printf("receving data...\n");
      
      int fd = open("text.txt", O_RDWR|O_CREAT, 0644);
      dup2(fd, STDOUT_FILENO);
      
      rc = recv(client_sock, buf, sizeof(buf), 0);

      if(rc == -1) {
        printf("RECEIVE ERROR: \n");
        close(server_sock);
        close(client_sock);
        exit(1);
      }
      else {
      printf("Data receve!\n");
      printf("Data : %s", buf);
      }

      close(server_sock);
      close(client_sock);

    }

  return 0;
}

void free_commands(int n_commands, struct single_command (*commands)[512])
{
  for (int i = 0; i < n_commands; ++i) {
    struct single_command *com = (*commands) + i;
    int argc = com->argc;
    char** argv = com->argv;

    for (int j = 0; j < argc; ++j) {
      free(argv[j]);
    }

    free(argv);
  }

  memset((*commands), 0, sizeof(struct single_command) * n_commands);
}
