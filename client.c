#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>

#define PORT 2828

#define MIN_ARGS 2
#define MAX_ARGS 2
#define SERVER_ARG_IDX 1

#define USAGE_STRING "usage: %s <server address>\n"

void validate_arguments(int argc, char *argv[])
{
    if (argc == 0)
    {
        fprintf(stderr, USAGE_STRING, "client");
        exit(EXIT_FAILURE);
    }
    else if (argc < MIN_ARGS || argc > MAX_ARGS)
    {
        fprintf(stderr, USAGE_STRING, argv[0]);
        exit(EXIT_FAILURE);
    }
}

void send_request(int fd)
{
   char *line = NULL;
   size_t size;
   ssize_t num;
   char output[1024];

   ssize_t bytes;
   char copy[1024];

   while ((num = getline(&line, &size, stdin)) >= 0) //sending requests
   {      
      write(fd, line, num);

      char *token;
      char *head = "HEAD";
      char *get = "GET";
      int req_type = 0;
      token = strtok(line, " \t\n"); //command type

      if(strcmp(token, head) == 0) {
         //printf("executing head request\n");
         req_type = 1;
      } else if(strcmp(token, get) == 0) {
         //printf("executing get request\n");
         req_type = 2;
      } else {
         //HTTP/1.0 400 Bad Request
         char err[1024];
         if(recv(fd, err, sizeof(err), 0) < 0) {
            perror("recv");
            close(fd);
            exit(1);
         }
         printf("%s\n", err);
         //printf("no request\n");
      }

      int lines = 0;
      if(req_type == 1) {
         lines = 4;
      } else if (req_type == 2) {
         char temp[1024];
         if(recv(fd, &temp, sizeof(temp), 0) != sizeof(temp)){
            perror("recv");
            close(fd);
            exit(1);
         }

         if(strstr(temp, "Error:") != NULL) {
            printf("%s\n", temp);
            close(fd);
            break;
         } else {
            lines = atoi(temp);
         }
         //recieve line number from server
      }
      //printf("lines determined: %d\n", lines);

      if(req_type >= 1) { //printing header
         while (lines > 0) {
            //printf("lines: %d\n", line);

            bytes = recv(fd, &output, sizeof(output), 0);
            if(bytes != sizeof(output)){
               perror("recv");
               close(fd);
               exit(EXIT_FAILURE);
            }

            strcpy(copy, output);

            if(strstr(copy, "Error:") != NULL) {
               printf("%s\n", copy);
               close(fd);
               free(line);
               //break;
               exit(1);
            } else {
               printf("%s\n", copy);
               lines--;
            }
         }
         //while line != 0, check recv, if error, exit, if not, subtract or add to line.
      }

      close(fd);
      //printf("exit loop\n");
      break;
   }

   free(line);
}

int connect_to_server(struct hostent *host_entry)
{
   int fd;
   struct sockaddr_in their_addr;

   if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      return -1;
   }
   
   their_addr.sin_family = AF_INET;
   their_addr.sin_port = htons(PORT);
   their_addr.sin_addr = *((struct in_addr *)host_entry->h_addr);

   if (connect(fd, (struct sockaddr *)&their_addr,
      sizeof(struct sockaddr)) == -1)
   {
      close(fd);
      perror(0);
      return -1;
   }

   return fd;
}

struct hostent *gethost(char *hostname)
{
   struct hostent *he;

   if ((he = gethostbyname(hostname)) == NULL)
   {
      herror(hostname);
   }

   return he;
}

int main(int argc, char *argv[])
{
   validate_arguments(argc, argv);
   struct hostent *host_entry = gethost(argv[SERVER_ARG_IDX]);

   if (host_entry)
   {
      int fd = connect_to_server(host_entry);
      if (fd != -1)
      {
         send_request(fd);
         close(fd);
      }
   }

   return 0;
}
