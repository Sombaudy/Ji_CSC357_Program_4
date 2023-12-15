#define _GNU_SOURCE
#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PORT 2828

void fork_request(char *request, int nfd) {
   pid_t child_pid;

   child_pid = fork();

   if (child_pid < 0) {
      char err[1024] = "Error: HTTP/1.0 500 Internal Error";
      send(nfd, err, sizeof(err), 0);
      perror("fork failed");
      exit(1);
      //HTTP/1.0 500 Internal Error
   } else if (child_pid == 0) {
      char *token;
      char *head = "HEAD";
      char *get = "GET";
      int req_type = 0;

      token = strtok(request, " \t\n"); //command type
      printf("request type: %s\n", token);

      if(strcmp(token, head) == 0) {
         printf("executing head request\n");
         req_type = 1;
      } else if(strcmp(token, get) == 0) {
         printf("executing get request\n");
         req_type = 2;
      } else {
         char err[1024] = "Error: HTTP/1.0 400 Bad Request";
         if (send(nfd, err, sizeof(err), 0)) {
            perror("send");
            exit(1);
         }
         //printf("no request\n");
         exit(1);
      }

      token = strtok(NULL, " \t\n"); //filename
      //printf("filename: %s\n", token);

      if (strstr(token, "..") != NULL) {
         char err[1024] = "Error: HTTP/1.0 403 Perimission Denied";
         send(nfd, err, sizeof(err), 0);
         perror("permission denied");
         exit(1);
         //HTTP/1.0 403 Permission Denied
      }

      FILE *file = fopen(token, "r");
      if (file == NULL) {
         char err[1024] = "Error: HTTP/1.0 404 Not Found";
         send(nfd, err, sizeof(err), 0);
         perror("Error opening file");
         fclose(file);
         exit(1);
         //HTTP/1.0 500 Internal Error
      }

      token = strtok(NULL, " \t\n"); //http version

      fseek(file, 0, SEEK_END); // Move file pointer to the end of the file
      long num_bytes = ftell(file); // Get the current position, which is the file size

      if (num_bytes == -1) {
         char err[1024] = "Error: HTTP/1.0 500 Internal Error";
         send(nfd, err, sizeof(err), 0);
         perror("ftell"); //ERROR
         fclose(file);
         exit(1);
      }
      if (fseek(file, 0, SEEK_SET) != 0) {
         char err[1024] = "Error: HTTP/1.0 500 Internal Error";
         send(nfd, err, sizeof(err), 0);
         perror("Error resetting file pointer"); //ERROR
         fclose(file);
         exit(1);
      }

      int lines = 0;

      if(req_type >= 1) {
         //write header
         char *header[10];
         lines = 4;
         char buffer[1024];

         if(req_type == 2) { //get and send total lines
            while (fgets(buffer, sizeof(buffer), file) != NULL) {
               lines++;
            }

            if (fseek(file, 0, SEEK_SET) != 0) {
               char err[1024] = "Error: HTTP/1.0 500 Internal Error";
               send(nfd, err, sizeof(err), 0);
               perror("Error resetting file pointer"); //ERROR
               fclose(file);
               exit(1);
            }

            char str[1024];
            sprintf(str, "%d", lines);

            if(send(nfd, &str, sizeof(str), 0) != sizeof(str)) {
               char err[1024] = "Error: HTTP/1.0 500 Internal Error";
               send(nfd, err, sizeof(err), 0);
               perror("send error"); //ERROR
               fclose(file);
               exit(1);
            }
         }

         header[0] = malloc(100 * sizeof(char));
         strcpy(header[0], "HTTP/1.0 200 OK\\r\\n");
         header[1] = malloc(100 * sizeof(char));
         strcpy(header[1], "Content-Type: text/html\\r\\n");
         header[2] = malloc(100 * sizeof(char));
         sprintf(header[2], "Content-Length: %ld\\r\\n", num_bytes);
         header[3] = malloc(100 * sizeof(char));
         strcpy(header[3], "\\r\\n");

         char copy[1024];

         for (int i = 0; i < 4; i++) {
            strcpy(copy, header[i]);

            if(send(nfd, copy, sizeof(copy), 0) != sizeof(copy)) {
               char err[1024] = "Error: HTTP/1.0 500 Internal Error";
               send(nfd, err, sizeof(err), 0);
               perror("send error"); //ERROR
               fclose(file);
               exit(1);
            }
            printf("send to client: %s\n", copy);
         }

         for (int i = 0; i < 4; i++) {
            //printf("freeing array\n");
            free(header[i]);
         }

         if(req_type == 2) {
            //do GET here
            while (fgets(buffer, sizeof(buffer), file) != NULL) {
               //printf("%s", buffer); // Print or process the line as needed
               strcpy(copy, buffer);

               size_t len = strlen(copy);
               if (len > 0 && copy[len - 1] == '\n') {
                     copy[len - 1] = '\0';
               }

               if(send(nfd, copy, sizeof(copy), 0) != sizeof(copy)) {
                  char err[1024] = "Error: HTTP/1.0 500 Internal Error";
                  send(nfd, err, sizeof(err), 0);
                  perror("send error"); //ERROR
                  fclose(file);
                  exit(1);
               }
               printf("send to client: %s\n", copy);
            }
            
         }

      }

      fclose(file);
      exit(0);
   } else {
      int status;
      waitpid(child_pid, &status, 0);
      printf("child process exited\n");
      return;
   }
}

void handle_request(int nfd)
{
   FILE *network = fdopen(nfd, "r");
   char *line = NULL;
   size_t size;
   ssize_t num;

   if (network == NULL)
   {
      perror("fdopen");
      close(nfd);
      return;
   }

   char copy[100];
   while ((num = getline(&line, &size, network)) >= 0) //recieve request
   {
      //printf("start new loop\n");
      //printf("received from client: %s", line);

      strcpy(copy, line);
      //printf("copy: %s", copy);

      fork_request(copy, nfd);

      printf("done forking\n");

      // if(write(nfd, copy, sizeof(copy)) != sizeof(copy)) {
      //    perror("write error");
      //    close(nfd);
      //    return;
      // }
   }

   //printf("exited loop\n");
   free(line);
   fclose(network);
}

void run_service(int fd)
{
   while (1)
   {
      int nfd = accept_connection(fd);
      if (nfd != -1)
      {
         printf("Connection established\n");
         handle_request(nfd);
         printf("Connection closed\n");
      }
   }
}

int main(void)
{
   int fd = create_service(PORT);

   if (fd == -1)
   {
      perror(0);
      exit(1);
   }

   printf("listening on port: %d\n", PORT);
   run_service(fd);
   close(fd);

   return 0;
}
