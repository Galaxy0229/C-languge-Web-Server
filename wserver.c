#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>    /* Internet domain header */

#include "wrapsock.h"
#include "ws_helpers.h"

#define MAXCLIENTS 10

int handleClient(struct clientstate *cs, char *line);


// You may want to use this function for initial testing
//void write_page(int fd);

int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: wserver <port>\n");
        exit(1);
    }
    unsigned short port = (unsigned short)atoi(argv[1]);
    int listenfd;
    struct clientstate client[MAXCLIENTS];


    // Set up the socket to which the clients will connect
    listenfd = setupServerSocket(port);

    initClients(client, MAXCLIENTS);



    // TODO: complete this function
    // The client accept - message accept loop. First, we prepare to listen to multiple
    // file descriptors by initializing a set of file descriptors.
    int max_fd = listenfd;
    fd_set all_fds;
    FD_ZERO(&all_fds);
    FD_SET(listenfd, &all_fds);
    struct timeval timeout;
    int count = 0;

    while (count < 10) {
        // fprintf(stderr, "one loop here\n");
        // select updates the fd_set it receives, so we always use a copy and retain the original.
        fd_set listen_fds = all_fds;
        // if (select(max_fd + 1, &listen_fds, NULL, NULL, NULL) == -1) {
        //     perror("server: select");
        //     exit(1);
        // }

        /* timeout in 5 mins = 5 * 60 = 300 seconds */
        timeout.tv_sec = 300;
        timeout.tv_usec = 0;  /* and microseconds */

        int nready = Select(max_fd + 1, &listen_fds, NULL, NULL, &timeout);
        // fprintf(stderr, "I am here\n");
        if (nready == 0) {
            // printf("No response from clients in %ld seconds\n", timeout.tv_sec);
            exit(0);
            
        }
        // printf("Time says %ld seconds\n", timeout.tv_sec);
        
        else if (nready > 0) {
            // if the new connection arrives then FD is set
            if (FD_ISSET(listenfd, &listen_fds)){
                for (int index = 0; index < MAXCLIENTS; index++){
                    printf("a new client is connecting\n");
                    if (client[index].sock == -1){
                        int clientfd = Accept(listenfd, NULL, NULL);
                        
                        client[index].sock = clientfd; 
                        if (clientfd > max_fd) {
                            max_fd = clientfd;
                        }  
                        FD_SET(clientfd, &all_fds);
                        break;
                    }
                }         
            }

            // Next, check the clients.
            for (int index = 0; index < MAXCLIENTS; index++) {
                // fprintf(stderr, "sock is %d\n",client[index].sock);
                if (client[index].sock > -1 && FD_ISSET(client[index].sock, &listen_fds)) {
                    // fprintf(stderr, "sock here!\n");
                    // read
                    int fd = client[index].sock;
                    char buf[MAXLINE + 1];
                    int client_closed = read(fd, &buf, MAXLINE);
                    buf[client_closed] = '\0';
                    
                    if (client_closed > 0) {
                        int result = handleClient(&client[index], buf);
                        // fprintf(stderr, "result is %d\n", result);
                        if (result == -1) {
                            int tmp_fd = client[index].sock ;
                            resetClient(&client[index]);
                            FD_CLR(tmp_fd, &all_fds);
                            FD_CLR(client[index].fd[0], &all_fds);
                            Close(tmp_fd);
                        }
                        if (result == 1){
                            int request = processRequest(client);
                            if (request == -1){
                                int tmp_fd = client[index].sock;
                                resetClient(&client[index]);
                                FD_CLR(tmp_fd, &all_fds);
                                FD_CLR(client[index].fd[0], &all_fds);
                                Close(tmp_fd);
                            }
                            else {
                                client[index].fd[0] = request;
                                FD_SET(client[index].fd[0], &all_fds);
                                if (client[index].fd[0] > max_fd) {
                                    max_fd = client[index].fd[0];
                                }
                            }                          
                        }
                    }
                }
                if (client[index].fd[0] > -1 && FD_ISSET(client[index].fd[0], &listen_fds)) {
                    // read
                    int fd = client[index].fd[0];
                    // char buf[MAXLINE + 1];
                    int ropt = read(fd, client[index].optr, MAXPAGE-(client[index].optr-client[index].output));
                    // buf[client_closed] = '\0';
                    client[index].optr += ropt; 

                    if (ropt<=0){
                        int status;
                        if (waitpid(client[index].pid, &status, 1) == -1){
                            perror("wait");
                        }
                        else {
                            if (WIFEXITED(status) && (WEXITSTATUS(status) == 0)){
                                int diff = client[index].optr-client[index].output;
                                printOK(client[index].sock, client[index].output, diff);
                            }
                            else if (WIFEXITED(status) && (WEXITSTATUS(status) == 100)){
                                printNotFound(client[index].sock);
                            }
                            else {
                                printServerError(client[index].sock);
                            }
                        }
                        // fprintf(stderr, "path is %s query is %s\n", client[index].path, client[index].query_string);
                        Close(client[index].fd[0]);
                        Close(client[index].sock);
                        FD_CLR(client[index].fd[0], &all_fds);
                        FD_CLR(client[index].sock, &all_fds);
                        resetClient(&client[index]);
                        count += 1;
                    }
                }
            }
        }
    }
    return 0;
}


/* Update the client state cs with the request input in line.
 * Intializes cs->request if this is the first read call from the socket.
 * Note that line must be null-terminated string.
 *
 * Return 0 if the get request message is not complete and we need to wait for
 *     more data
 * Return -1 if there is an error and the socket should be closed
 *     - Request is not a GET request
 *     - The first line of the GET request is poorly formatted (getPath, getQuery)
 * 
 * Return 1 if the get request message is complete and ready for processing
 *     cs->request will hold the complete request
 *     cs->path will hold the executable path for the CGI program
 *     cs->query will hold the query string
 *     cs->output will be allocated to hold the output of the CGI program
 *     cs->optr will point to the beginning of cs->output
 */
int handleClient(struct clientstate *cs, char *line) {
    // TODO: Complete this function
    if (cs->request == NULL){
        cs->request = malloc(sizeof(char) * MAXLINE);
        cs->request[0] = '\0';
    }
    
    // int left = MAXLINE - strlen(cs->request) - 1;
    strcat(cs->request, line);
    cs->request[MAXLINE] = '\0';

    char web_str[5] = "\r\n\r\n\0";
    // strstr(cs->request, web_str)!= NULL
    // cs->request[i] == '\r' && cs->request[i+1] == '\n' && 
    //         cs->request[i+2] == '\r' && cs->request[i+3] == '\n'
    for (int i = 0; i < MAXLINE - 3; i++){
        // fprintf(stderr, " %d ", cs->request[i]);
        if (strstr(cs->request, web_str)!= NULL) {
            // fprintf(stderr, "request here\n");
            if (cs->request[0] != 'G' || cs->request[1] != 'E' || cs->request[2] != 'T'){
                return -1;
            }
            else {
                cs->path = getPath(cs->request);
                if (cs->path == NULL){
                    return -1;
                }
                cs->query_string = getQuery(cs->request);
                cs->output = malloc(MAXPAGE * sizeof(char));
                cs->output[0] = '\0';
                cs->optr = cs->output;
                return 1;
            }
        }
    }
    return 0;
 
    // If the resource is favicon.ico we will ignore the request
    if(strcmp("favicon.ico", cs->path) == 0){
        // A suggestion for debugging output
        fprintf(stderr, "Client: sock = %d\n", cs->sock);
        fprintf(stderr, "        path = %s (ignoring)\n", cs->path);
		printNotFound(cs->sock);
        return -1;
    }


    // A suggestion for printing some information about each client. 
    // You are welcome to modify or remove these print statements
    fprintf(stderr, "Client: sock = %d\n", cs->sock);
    fprintf(stderr, "        path = %s\n", cs->path);
    fprintf(stderr, "        query_string = %s\n", cs->query_string);

    return 1;
}

