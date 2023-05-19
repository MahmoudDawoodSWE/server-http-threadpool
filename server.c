#include  <stdio.h>   // printf, perror ...
#include  <stdlib.h>  // atoi, free, malloc, realloc ...
#include  "threadpool.h" // dispatch, create_threadpool ...
#include  <string.h> // strcpy, strcmp, stlen, strstr, strtok, strrchr ...
#include  <sys/stat.h> // state, lstate ...
#include  <dirent.h> // closedir, opendir ...
#include  <unistd.h> // close, write, read ...
#include  <fcntl.h> // open ...
#include <sys/socket.h> // socket, bind, listen, accept ...
#include <arpa/inet.h> // INADDR_ANY, htonl


#define MAX_NUM_IN_WELCOME_SOCKET 6 // the max number can the server get in the same time  used by bind
#define FAILED (-1) // fail value
#define SUCCEEDED 0 // success value
#define BUFFER_LEN 700 // buffer length
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT" // time shape as in the response

//thread job/tasks
int thread_job( void* parameter ); //  the thread main function decide what to do "uses the functions below"
void makeAndWriteError(int FD, int errorCode, char * pathForErrorFound); // send error "write" to the socket
int writeFileContent( int FD, char * path, struct stat stateOfTheFile ); // send "write" to the socket file "photo, video, txt .. "
int writeTableDirectoryContent( int FD, char * path, struct stat stateOfTheFile ); // send  "write" to the socket folder content
int checkSymbolicLink(char * path, int length);//check all the file in symbolic link if they have permethrin to search
char * get_mime_type(char * name); // get file to send type
//main thread job/tasks
int welcomeSocket( int ); // set up the welcome socket
int isNum( char*, int); //  check if the string is content just digit
int main( int argc, char * argv[] ) {

    if ( argc > 4 || argc < 4 ) {// Make sure that we get the required number of  parameter
        printf( "\nusage: server <port> <pool-size> <max-number-of-request>\n" );
        exit( 0 );
    }/*
     * pool
     */
    threadpool * pool;
    int welcomeSocketFD; // the stream in the file descriptor table of the welcome socket
    /*
     * new socket value
     */
    int handleRequestSocket; // new stream socket in the file descriptor table
    int *passValue = NULL;
    /*
     * input
     */
    int welcomePort; // This is the port that we will be listening to welcome request
    int threadPoolSize; // the number of the thread in the pool
    int maxNumberOfRequest; // The maximum number of request that server can handle

    /*
     * check
     */
    for ( int i = 1; i < 4; i++ ) { // if one of them with no digit value
        if ( isNum( argv[ i ], strlen( argv[ i ] ) ) == FAILED ){
            printf( "\nusage: server <port> <pool-size> <max-number-of-request>\n" );
            exit( 0 );
        }
    }
    /*
     * assign
     */
    welcomePort = atoi( argv[ 1 ] ); // the port will be in cell 2
    threadPoolSize =  atoi( argv[ 2 ] ); // the poolSize will be in cell 3
    maxNumberOfRequest =  atoi( argv[ 3 ]); // the poolSize will be in cell 4
    /*
     * check value
     */
    if (  welcomePort < 0 ){ // the port number should not be under 0
        printf( "\nusage: server <port> <pool-size> <max-number-of-request>\n" );
        exit( 0 );
    }
    if ( welcomePort > 65536 ){ // the port number should be under 2^16
        printf( "\nusage: server <port> <pool-size> <max-number-of-request>\n" );
        exit( 0 );
    }

    if (  threadPoolSize > MAXT_IN_POOL ) { // if threadPoolSize too big
        printf( "\nusage: server <port> <pool-size> <max-number-of-request>\n" );
        exit( 0 );
    }
    if (  threadPoolSize < 0  ) { // there  is no thread !
        printf( "\nusage: server <port> <pool-size> <max-number-of-request>\n" );
        exit( 0 );
    }

    if ( maxNumberOfRequest < 0 ){ // no request
        printf( "\nusage: server <port> <pool-size> <max-number-of-request>\n" );
        exit( 0 );
    }

    if ( ( pool = create_threadpool( threadPoolSize ) ) == NULL ) { // this the pool "holed the threads"
        return 1;
    }

    if ( ( welcomeSocketFD = welcomeSocket( welcomePort ) ) != FAILED ) { // start the welcome socket
        for ( int i = 0; i < maxNumberOfRequest; i++ ) { // we have limit to on  request  number
            if ( ( handleRequestSocket = accept( welcomeSocketFD, NULL, NULL ) ) == FAILED ){ // welcome request
                perror( "\naccept\n" );
                destroy_threadpool( pool );
                return 1;
            } else {
                passValue = (int*) malloc(sizeof (int)); // pass the value to the heap
                if (passValue != NULL){
                    /*
                     * attach the job  to thread
                     */
                    *passValue = handleRequestSocket;
                    dispatch( pool, thread_job, ( void* ) passValue);
                }else{
                    printf("\nmalloc\n");
                    destroy_threadpool( pool );
                    return 1;
                }
                passValue = NULL;
            }
        }
    }
    else  if( welcomeSocketFD == FAILED ){
        destroy_threadpool( pool );
        return 1;
    }

    /*
     *
     * If we get here we have to finish and collect the threads
     * close the welcome socket
     *
     */
    destroy_threadpool( pool );
    close( handleRequestSocket );
    return 0;
}

int thread_job( void * parameter ) { // the thread job for clint
    int FD =   *   (int*)   parameter; // get the fd that we receive from the main thead
    free(parameter);
    char httpRequest[BUFFER_LEN]; // buffer to read the request from the socket
    char path[BUFFER_LEN]; // buffer to get the path from the request
    path[0] ='\0';
    struct stat stateOfTheFile; // stat is a struct with detail about the fail
    /*
     * first we read the httpRequest from the socket
     */

    int return_read_val = 1;
    while ( return_read_val > 0 ) { // still there is something to read
        return_read_val =(int) read( FD, httpRequest, BUFFER_LEN ); // read
        httpRequest[ return_read_val ] = '\0'; // set buffer end
        if ( return_read_val == FAILED ){ // if read failed
            perror( "\nread\n" );
            makeAndWriteError( FD, 500, NULL );
            close(FD); // close the socket "communication"
            return FAILED; // stop the work
        }
        else{ // read did not fail
            if ( strstr( httpRequest, "\r\n" ) ) // if we see "\r\n" we stop this means we read enough to get the path
                break;
        }

    }
    /*
     *  strstr return the pointer to char that founded
     */
    char *index= strstr( httpRequest,"\r\n" );
    if(index != NULL)
        *index='\0';
    else{
        makeAndWriteError( FD, 500, NULL );
        close( FD ); // close the socket "communication"
        return FAILED; // stop the work
    }
    // if the length is too short
    if( strlen(httpRequest) <= strlen("GET HTTP/1.1") ){
        makeAndWriteError( FD, 400, NULL ); // bad request
        close( FD ); // close the socket "communication"
        return FAILED; // stop the work
    }
    /*
     * check that  httpRequest end with HTTP/1.1 OR HTTP/1.0
     */
    int i = strlen( httpRequest ) - 1;
    if ( httpRequest[ i ] != '0' && httpRequest[ i ] != '1' ){
        makeAndWriteError( FD, 400, NULL ); // bad request
        close( FD ); // close the socket "communication"
        return FAILED; // stop the work
    }
    i--;
    for ( int j = 6; i > 3 && j >= 0; --i , --j) {
        if("HTTP/1.1"[j] != httpRequest[i]){
            makeAndWriteError( FD, 400, NULL ); // bad request
            close( FD ); // close the socket "communication"
            return FAILED; // stop the work
        }
    }
    i = 0;
    for ( ; i < strlen("GET ") ; ++i ) {
        if( httpRequest[i] != "GET "[i] ) {
            makeAndWriteError(FD, 501, NULL); // not supported
            close(FD); // close the socket "communication"
            return FAILED; // stop the work
        }
    }


    /*
    * we do this to have path like this "./<path>" so we can start search from this directory "project"
    */
    httpRequest[strlen(httpRequest) - 9] = '\0'; // cut Http/1.<1 or 0> from the end
    strcpy(path, &httpRequest[4]); // copy the request into the path without the first "GET " char
    strcpy(httpRequest, "."); // copy the point to request
    strcat(httpRequest, path); // copy the path after the point
    strcpy(path, httpRequest); // set the last result into the path

    /*
     * get the information about the file and check if not found
     */

    if ( stat( path, & stateOfTheFile ) == FAILED ) { // check the permeation of the file itself
        makeAndWriteError( FD, 404, NULL ); // not Found
        close( FD ); // close the socket "communication"
        return FAILED; // stop the work
    }
    /*
     * check the directory on the way execute  permeation
     *
     * for example if the link is ./directory1/directory2/file
     *
     * we check ./directory1/directory2/ execute  permeation for other
     *
     * ./directory1/ execute  permeation for other
     *
     * and ./ execute  permeation for other
     */
    if ( checkSymbolicLink( httpRequest, strlen( path ) ) == FAILED ){
        makeAndWriteError( FD, 403, NULL ); // forbidden
        close( FD ); // close the socket "communication"
        return FAILED; // stop the work
    }

    if ( S_ISDIR( stateOfTheFile.st_mode ) ) { // the path is directory
        int flag_found_html = 0; // flag to send directory in certain conditions
        struct dirent * fileInt; // to run over the file into the directory
        DIR * directory;
        if( !( stateOfTheFile.st_mode & S_IXOTH ) ){ // execute permeation we cant get the files into it
            makeAndWriteError( FD, 403, NULL ); // forbidden
            close( FD ); // close the socket "communication"
            return FAILED; // stop the work
        }
        else if ( ( directory = opendir( path ) ) == NULL) {
            makeAndWriteError( FD, 500, NULL ); // internal Server Error
            close( FD ); // close the socket "communication"
            return FAILED; // stop the work
        } else if ( path[ strlen( path ) - 1 ] != '/') { // the path is dir without "/" at last char
            makeAndWriteError( FD, 302, &path[ 1 ] ); // found  delete the point from the first place
            closedir( directory );
            close( FD ); // close the socket "communication"
            return FAILED; // stop the work
        } else { // check if the directory contain  html file
            while ( ( fileInt = readdir( directory ) ) != NULL ) { // next file
                if ( strcmp( fileInt -> d_name, "index.html" ) == 0) { // founded
                    strcat(path, "index.html"); // add the file name to the path
                    stat( path, &stateOfTheFile ); // reset stat value
                    flag_found_html = 1;
                }
            }
            closedir( directory );
        }
        if ( flag_found_html == 0 ) {
            if ( writeTableDirectoryContent( FD, path, stateOfTheFile ) == FAILED ) { // if we fail at sending
                makeAndWriteError(FD, 500, NULL); // internal Server Error
                close( FD ); // close the socket "communication"
                return FAILED; // stop the work
            }
        }
    }

    if ( S_ISREG( stateOfTheFile.st_mode ) ) { // if the path is a file
        /*
         * user group and other should have read permeation
         */
        if(!( stateOfTheFile.st_mode & S_IRUSR ) || !( stateOfTheFile.st_mode & S_IRGRP ) || !( stateOfTheFile.st_mode & S_IROTH )){
            makeAndWriteError(FD, 403, NULL); // forbidden
            close(FD);
            return FAILED;
        }
        else if ( writeFileContent(FD, path, stateOfTheFile) == FAILED ) {
            close(FD);
            return FAILED;
        }
    }

    close(FD);
    return SUCCEEDED;
}



void makeAndWriteError( int FD, int errorCode, char * pathForErrorFound ) {
    char theMessage[ ( BUFFER_LEN * 2 ) + 50];
    char newPath[BUFFER_LEN]; // holed the new path in case of error 302 Found
    char error_name[50]; // holed the error name
    // time set up
    char timebuf[128];
    time_t now = time( NULL );
    strftime( timebuf, sizeof( timebuf ), RFC1123FMT, gmtime( &now ) );
    if (pathForErrorFound != NULL)
        sprintf(newPath, "Location: %s/\r\n", pathForErrorFound);
    /*
     * here we bulled the error name
     */
    sprintf(error_name, "%d %s", errorCode,
            errorCode == 400 ? "Bad Request" :
            errorCode == 404 ? "Not Found" :
            errorCode == 403 ? "Forbidden" :
            errorCode == 500 ? "Internal Server Error" :
            errorCode == 501 ? "Not supported" :
            errorCode == 302 ? "Found" : " ");

    sprintf( theMessage, "HTTP/1.1 %s\r\n"
                               "Server: webserver/1.0\r\n"
                               "Date: %s\r\n"
                               "%s"
                               "Content-Type: text/html\r\n"
                               "Content-Length: %s\r\n"
                               "Connection: close\r\n\r\n"
                               "<HTML><HEAD><TITLE>%s</TITLE></HEAD>\r\n"
                               "<BODY><H4>%s</H4>\r\n"
                               "%s\r\n"
                               "</BODY></HTML>\r\n",
             error_name, // name
             timebuf, // time
             errorCode == 302 ? newPath : "",//the new path
             errorCode == 302 ? "123" :
             errorCode == 400 ? "113" :
             errorCode == 404 ? "112" :
             errorCode == 500 ? "144" :
             errorCode == 403 ? "111" :
             errorCode == 501 ? "129" :"",
            error_name, error_name,
            errorCode == 400 ? "Bad Request." :
            errorCode == 404 ? "File not found." :
            errorCode == 500 ? "Some server side error." :
            errorCode == 403 ? "Access denied." :
            errorCode == 501 ? "Method is not supported." :
            errorCode == 302 ? "Directories must end with a slash." : "");



    if ( write( FD, theMessage, strlen( theMessage ) ) == FAILED ) { // write the headers
        perror( "\nwrite\n" );
        return;
    }

}

int writeFileContent( int FD, char * path, struct stat stateOfTheFile ) {
    //to get the time
    char timebuf[ 128 ]; // Date : .....
    char timebuf1[ 128 ]; // Last-Modified: ....
    //to send, check, read  the file
    int FD_file; // file descriptor stream
    char * mime_type; // file type
    char answer_header[ BUFFER_LEN ]; // headers buffer
    char buffer_data[ BUFFER_LEN ]; // data of the file
    int readedIntoBuffer; // number of byte read successfully
    char content_type[ 50 ]; // holed the content type
    // get file type
    if ( ( mime_type = get_mime_type(path ) ) != NULL ) {
            sprintf(content_type, "Content-Type: %s\r\n", mime_type);
    }
    // set the time into timebuf, timebuf1
    time_t now = time(NULL);
    strftime( timebuf, sizeof( timebuf ),
              RFC1123FMT, gmtime( & now ) );
    strftime( timebuf1, sizeof( timebuf1 ),
              RFC1123FMT, gmtime( & stateOfTheFile.st_mtime ) );

    // open the file stream
    if ( ( FD_file = open( path, O_RDONLY) ) == FAILED ) {
        makeAndWriteError( FD, 500, NULL ); // internal Server Error
        perror( "\nopen\n" );
        return FAILED;
    }
    // make the header answer
    sprintf( answer_header,
             "HTTP/1.1 200 OK\r\n"
             "Server: webserver/1.0\r\n"
             "Date: %s\r\n"
             "%s"
             "Content-Length: %d\r\n"
             "Last-Modified: %s\r\n"
             "Connection: close\r\n\r\n",
             timebuf, mime_type != NULL ? content_type : "",   (int) stateOfTheFile.st_size,   timebuf1);
    if ( write( FD, answer_header, strlen(answer_header ) ) == FAILED ) {
        close( FD_file ); // close the stream of the file
        makeAndWriteError( FD, 500, NULL ); // internal Server Error
        perror( "\nwrite\n" );
        return FAILED;
    }
    /*
     * read from the file and write into the socket
     */
    while ( ( readedIntoBuffer = ( int ) read( FD_file, buffer_data, BUFFER_LEN ) ) != 0) {
        if ( write( FD, buffer_data, readedIntoBuffer ) == FAILED ) {
            close( FD_file ); // close the stream of the file
            makeAndWriteError( FD, 500, NULL ); // internal Server Error
            perror( "\nwrite\n" );
            return FAILED;
        }
    }
    if(readedIntoBuffer == FAILED){
        close( FD_file ); // close the stream of the file
        makeAndWriteError( FD, 500, NULL ); // internal Server Error
        perror( "\nread\n" );
        return FAILED;
    }
    // finish sending
    if ( write(  FD, "\r\n\r\n", 4 ) == FAILED ) {
        close( FD_file ); // close the stream of the file
        makeAndWriteError( FD, 500, NULL ); // internal Server Error
        perror( "\nwrite\n" );
        return FAILED;
    }
    close( FD_file ); // close the stream of the file
    return SUCCEEDED;
}

int writeTableDirectoryContent( int FD, char * path, struct stat stateOfTheDirectory ) {
    //to get the time
    char timebuf[ 128 ]; // Date : .....
    char timebuf1[ 128 ]; // Last-Modified: ....
    char answer_header[ BUFFER_LEN ]; // headers buffer
    DIR * directory; // directory
    char * directory_table; // the html code to send
    //these buffers used to get information about the files into the directory
    char buffer[ BUFFER_LEN * 2 ]; // for the path and new row
    struct dirent * fileEnter; // run over the files
    struct stat tempState; // details
    int i = 1; // size to be reallocated
    char sizeFile[10];

    // set the time into timebuf, timebuf1
    time_t now = time(NULL);
    strftime( timebuf, sizeof( timebuf ),
              RFC1123FMT, gmtime( & now ) );
    strftime( timebuf1, sizeof( timebuf1 ),
              RFC1123FMT, gmtime( & stateOfTheDirectory.st_mtime ) );

    // start the table
    if ( ( directory_table = ( char * ) malloc(BUFFER_LEN * sizeof( char ) ) ) == NULL ) {
        makeAndWriteError( FD, 500, NULL ); // internal Server Error
        return FAILED;
    }

    sprintf(directory_table,
            "Connection: close\r\n\r\n<HTML>\r\n"
            "<HEAD><TITLE>Index of %s</TITLE></HEAD>\r\n"
            "<BODY>\r\n"
            "<H4> Index of %s </H4>\r\n"
            "<table CELLSPACING = 8 >\r\n"
            "<tr>\r\n"
            "<th>Name</th>\r\n"
            "<th>Last Modified</th>\r\n"
            "<th>Size</th>\r\n"
            "</tr>\r\n",path, path);

    // open the directory
    if ( ( directory = opendir(path) ) == NULL ) {
        makeAndWriteError( FD, 500, NULL ); // internal Server Error
        return FAILED;
    }
    char *fileName;
    while ( ( fileEnter = readdir( directory ) ) != NULL ) {
        /*
         * we have here new row so reallocate the size of the directory_table
         */
        if ( ( directory_table = ( char * ) realloc(directory_table, i * BUFFER_LEN * sizeof( char ) ) ) == NULL ) {
            return FAILED;
        }
        // add row
        i++;
        fileName = fileEnter -> d_name;
        // zero the buffer of new path of the new file
        bzero( buffer, BUFFER_LEN );
        // copy to tempPath  the directory path
        strcpy( buffer, path );
        // add the file name
        strcat(buffer, fileName );
        // get the file information
        stat( buffer, & tempState );
        // modification time
        strftime( timebuf, sizeof( timebuf ), RFC1123FMT, gmtime( & tempState.st_mtime ) );
        sprintf(sizeFile ,"%d", (int) tempState.st_size);
        // make  new row
        sprintf( buffer, "<tr>\n"
                         "<td><A HREF=\"%s\">%s</A></td>\n"
                         "<td>%s</td>\n"
                         "<td>%s</td>\n"
                         "</tr>\n", fileName,
                         fileName, timebuf,
                 S_ISREG( tempState.st_mode ) ? sizeFile : "");
        // add  new row
        strcat( directory_table, buffer );
    }
    // set the end
    strcat( directory_table, "</table\r\n"
                             "<HR>\r\n"
                             "<ADDRESS>webserver/1.0</ADDRESS>\r\n"
                             "</BODY></HTML>\r\n" );

    // headers maker
    sprintf( answer_header, "HTTP/1.1 200 OK\r\n"
                            "Server: webserver/1.0\r\n"
                            "Date: %s\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: %d\r\n"
                            "Last-Modified: %s\r\n",
             timebuf, (int)strlen(directory_table) -21, timebuf1 );


    if ( write( FD, answer_header, strlen( answer_header ) ) == FAILED ) { // write headers
        makeAndWriteError( FD, 500, NULL ); // internal Server Error
        perror( "\nwrite\n" );
        return FAILED;
    }
    if ( write( FD, directory_table, strlen( directory_table ) ) == FAILED ) { // write table
        makeAndWriteError( FD, 500, NULL ); // internal Server Error
        perror( "\nwrite\n" );
        return FAILED;
    }



    free(directory_table);
    closedir(directory);
    return SUCCEEDED;
}
int checkSymbolicLink(char * path, int length) {

    struct stat checkWay;
    int i = length;

    for (; i > 2; --i) {
        while ( path[i] != '/' ) {
            path[i] = '\0';
            i--;
        }
        stat( path, &checkWay );
        if( !( checkWay.st_mode & S_IXOTH ) ){
            return FAILED;
        }

        if (i == 1 && path[i - 1] == '.' && path[i] == '/') // stop  no more to check
             break;

    }
    return  SUCCEEDED;
}


int welcomeSocket( int welcomeSocketPort ) { // set up the welcome socket
    int welcomeSocket;
    if ( ( welcomeSocket = socket(AF_INET, SOCK_STREAM, 0)) == FAILED ) {
        perror("\n fail making the welcome socket \n");
        return FAILED;
    }
    //set up the address
    struct sockaddr_in srv;
    srv.sin_family = AF_INET;
    srv.sin_port = htons( welcomeSocketPort );
    srv.sin_addr.s_addr = htonl( INADDR_ANY );
    // bind it with the address
    if ( bind( welcomeSocket, ( struct sockaddr * ) & srv, sizeof( srv ) ) == FAILED) {
        perror("\n fail bind the welcome socket \n");
        return FAILED;
    }
    // listen make the number of the request that I can get at the same time
    if ( listen( welcomeSocket, MAX_NUM_IN_WELCOME_SOCKET) == FAILED ) {
        perror("\n fail to set listen \n");
        return FAILED;
    }
    return welcomeSocket;
}

int isNum( char * str, int len ){ // check if "number"
    int j = 0;
    while (j < len) {
        if (str[j] > 57 || str[j] < 48) {
            return FAILED;
        }
        j++;
    }
    return SUCCEEDED;
}

char *get_mime_type( char *name )
{
    char *ext = strrchr( name, '.' );
    if ( !ext ) return NULL;
    if ( strcmp( ext, ".html" ) == 0 || strcmp( ext, ".htm" ) == 0 ) return "text/html";
    if ( strcmp( ext, ".jpg" ) == 0 || strcmp( ext, ".jpeg" ) == 0 ) return "image/jpeg";
    if ( strcmp( ext, ".gif" ) == 0 ) return "image/gif";
    if ( strcmp( ext, ".png" ) == 0 ) return "image/png";
    if ( strcmp( ext, ".css" ) == 0 ) return "text/css";
    if ( strcmp( ext, ".au" ) == 0 ) return "audio/basic";
    if ( strcmp( ext, ".wav" ) == 0 ) return "audio/wav";
    if ( strcmp( ext, ".avi" ) == 0 ) return "video/x-msvideo";
    if ( strcmp( ext, ".mpeg" ) == 0 || strcmp( ext, ".mpg" ) == 0 ) return "video/mpeg";
    if ( strcmp( ext, ".mp3" ) == 0 ) return "audio/mpeg";
    return NULL;
}