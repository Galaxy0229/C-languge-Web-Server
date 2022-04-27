# C-languge-Web-Server
Server behaviour
The web server will accept the same kinds of requests as in assignment 3. It will only handle GET requests, and it can safely ignore other header lines. Please see the Assignment 3 description for more information about how the server will run CGI programs.

Your server will buffer the output from the child until it receives the termination status from the child. If the server detects an error, it will write to the socket the HTTP response line for the error, followed by a valid HTML page that reports the error. The web browser will show the error page.  The code to create and execute the CGI program comes from A3 and will be provided.

Details
Normally a web server would run in an infinite loop. However, in an effort to limit security risks, your web server should handle 10 connections and then terminate. It should be able to handle more than one request at a time. This will be accomplished using select to ensure that the server does not block waiting for a single read or accept request.

It is recommended that you get the socket and select functions working with a web browser as input before integrating it with your code from assignment 3. One way to do this is to send a hard-coded web page back to the client when the GET request is received rather than forking and exec'ing a program. See simplepage.c for an example of a function that just produces a web page that you can write to a socket descriptor.

The starter code in wserver.c carries out the initial steps:

wserver takes a port number as an argument. This is the port number on which the server will be accepting connections.
It sets up the socket for accepting connections.
It initializes the array that will hold client data. See the struct definition in ws_helpers.h for the details of the clientstate struct
Your task will be to complete the server code that manages the socket and pipe connections. You will use select to ensure that the server does not block on a accept or read call from a socket or a read call from a pipe. This means that both socket and pipe descriptors will need to be added to the read set monitored by select.

Your program should accept 10 new socket connections and then terminate with an exit value of 0. In addition, a select call should time out and exit the program with an exit value of 0 if it has been idle for 5 minutes. (These restrictions are intended to prevent students from leaving infinitely running processes lying around.) There are three types of file descriptors monitored by  select:

Listening socket descriptor:
This is the socket description for accepting a new connection
When a new connection arrives the server will add it to the clientstate array, and will add the new descriptor to the select set.
Client socket:
Read an HTTP request from a client
Recall that an HTTP request ends with a blank line.  This means that when the server receives a "\r\n\r\n" the request message is complete and can be processed.  The server should read from the socket until the blank line is received, accumulated parts of the request in the clientstate struct. Remember that the program can only call read once after select says that a read on the descriptor will not block.
When a complete message has been accumulated in the request field of the clientstate struct, determine how to handle it:
If the message is not a GET request, then ignore the message, close the client socket and clean up resources.
If the message is a GET request, then initialize the client using information from line .
If the path is for "favicon.ico", then ignore the message, close the client socket and clean up resources.
If everything is okay with the message then process the request
First, check if the program to execute is valid (in the set of allowed programs)
If it is valid, create a new process, set up the pipe, and calling exec on the program
Pipe descriptor:
Read output from a CGI program
Each read from a pipe may be adding data to the output buffer. If read returns 0 that means that the CGI process has closed the pipe and the server can do the same.
If there is no more data to read, the server will check the termination status of the child process and use it to write the appropriate HTTP response message to the client socket.
Remember to clean up resources. Close the socket, remove the socket descriptor from the select set.
 

HTTP Response
If the exec call succeeds and the exit value is 0, then the response is "200 OK" message plus the output from the pipe. If the exec call fails for any reason the child process should exit with the exit value 100 and the HTTP response will be "404 Not Found". If there is any other error along the way, the response should be "500 Internal Server Error". Internal server errors include the child process exiting with any exit code other than 0 or 100, the child process exiting due to a signal.  Use the provided functions.

Client data structure
The clientstate struct includes the socket to write to, the pipe file descriptors, and a pointer to the character array that will hold the data that is buffered from the CGI process.

The optr argument may require some further explanation. The client struct contains a pointer to the beginning of the buffer(output), and optr is a pointer to the current location in the buffer to read into. When optr is updated correctly, then a read call will read directly into the buffer at the correct position. At the end of each read call, the current pointer will need to be updated. For example:

    if ( (n = read(pd, client[i].optr, MAXLINE)) <= 0) {
        // Missing code here
    } else {
        client[i].optr += n;
    }
Later, the pointer to the beginning of the fully buffered output can be passed to printOK so that the output can be written to the client.

To accumulate the request message in the request field of the clientstate struct, ensure that string in the request field is null-terminated.

Choosing a port number
You need some way of choosing a port number to allow a web browser to connect to your server. DO NOT use port 80 which is the standard port for web servers. Instead, use the last 5 digits of your student number or something similarly random. The port number will be an argument to the program.

If you see the "bind error: address in use" that means that a process is already bound to that port number and you will have to choose another port number.

Important security details
Running a web server program on the teach.cs machines (or your own machine) opens up potential security risks. Anyone doing a port scan on the teach.cs machines may discover your server running and try to use it to damage your account or the teach.cs system. For example, it is possible for a malicious user to send GET //tmp/rmhome HTTP/1.0 and have written rmhome to remove everything in your account. Remember that your web server is running with your user id, but anyone can request a web page from your server. This means that it is extremely important that you adhere to the following requirements.

Never leave your server running when you are not actively working on it. The program killall takes a program name as an argument and tries to terminate it. You should also become familiar with ps and/or top to check for running processes. Before you log out of a teach.cs machine, run ps -aux | grep wserver to make sure you have not accidentally left a server process running.

Your web server must only exec programs in the same directory as the web server. You must use execl or execv to execute the CGI program. You must also check the resource string in the HTTP request to be sure that it refers to one of the "allowed" programs. You are given a function, validResource(char *str) that you must call to help ensure that your server only execs one of the programs that it is allowed to. Please see the comments in progtable.c.

Running and Testing
If you run a web browser (for example,firefox, chrome, lynx) on the same machine as your web server program, you can connect to your web server by entering a url as http://127.0.0.1:ppppp/prog or http://localhost:ppppp/prog where ppppp is replaced by the port number that your server is listening on, and prog is the name of the program you want to run. The address 127.0.0.1 and name localhost identify the machine on which the browser is running.

You can also use the nc program to act as a client to your web server. Run it as "nc localhost ppppp" where ppppp is the port number of your web server. Then type in your GET request, and you will see the output in text form.  Remember to use either "-c" or "-C" to send the network newlines. Which one to use depends on what machine you are running on ("-C" on teach.cs). Also remember that an HTTP request message is terminated by a blank line, so you will have to hit "enter" twice to send "\r\n\r\n"
