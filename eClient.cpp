#include "stdio.h"
#include "winsock.h"
#include "windows.h"
 
#define BUFFER_SIZE 0x400
#define REQUIRED_WINSOCK_VER 0x0101
#define WINSOCK_SUCCESS 0
 
const char COMMAND_CLOSE[] = "-close";
const char CONFIG_FILENAME[] = "config.cfg";
 
const char DEFAULT_INET_ADDR[] = "192.168.1.100";
const int DEFAULT_LISTENING_PORT = 23;
 
void exit_error_in_function( char function[] )
{
    system( "cls" );
    int error = WSAGetLastError();
    printf( "Function %s ERROR %i\n", function, error );
    exit( error );
    system( "pause" );
}
 
void fill_connection_data( struct sockaddr* src, BOOL preset )
{
    static FILE* config;
    static char file_buff[ 0x10 ];
    memset( src, 0, sizeof( *src ) );
 
    if( FALSE == preset )
    {
        config = fopen( CONFIG_FILENAME, "r" );
        if(FALSE == config)
        {
            preset = TRUE;
        }
        else
        {
            ( (struct sockaddr_in*)src )->sin_family = AF_INET;
 
            memset( file_buff, 0, sizeof(file_buff) );
            fgets( file_buff, 16, config );
            ( (struct sockaddr_in*)src )->sin_addr.S_un.S_addr = inet_addr( file_buff );
 
            memset( file_buff, 0, sizeof(file_buff) );
            fgets( file_buff, 16, config );
            ( (struct sockaddr_in*)src )->sin_port = htons( atoi( file_buff ) );
        }
 
 
    }
    if( TRUE == preset )
    {
        ( (struct sockaddr_in*)src )->sin_family = AF_INET;
        ( (struct sockaddr_in*)src )->sin_addr.S_un.S_addr = inet_addr( DEFAULT_INET_ADDR );
        ( (struct sockaddr_in*)src )->sin_port = htons( DEFAULT_LISTENING_PORT );
    }
}
 
void print_connecting( struct sockaddr_in* info )
{
    system( "cls" );
    printf( "Connecting to %s:%i...\n", inet_ntoa( info->sin_addr ), ntohs( info->sin_port ) );
}
 
int input_message(char* send_buff)
{
    static char input_buff[ BUFFER_SIZE ];
    static int sending_bytes;
 
    memset(&send_buff[0], 0, BUFFER_SIZE);
    sending_bytes = 0;
 
    printf(">>>");
    scanf( "%s", &input_buff );
    if( 0 != strcmp( input_buff, COMMAND_CLOSE ) )
    {
        memcpy(&send_buff[0], &input_buff[0], BUFFER_SIZE);
        sending_bytes = strlen(send_buff);
    }
 
    return sending_bytes;
}
 
int main()
{
    char buff[ BUFFER_SIZE ];
    SOCKET client_socket;
    struct sockaddr_in client_addr_info;
    int received_bytes;
    int msg_len;
    BOOL use_default_settings;
    char choice;
 
    if ( WINSOCK_SUCCESS != WSAStartup( REQUIRED_WINSOCK_VER, (WSADATA*)&buff[0] ) )
    {
        exit_error_in_function( "WSAStartup" );
    }
 
    client_socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if ( INVALID_SOCKET == client_socket )
    {
        exit_error_in_function( "socket" );
    }
 
    printf( "Read settings from config.cfg? (y/n) " );
    choice = getch();
    switch( tolower( choice ) )
    {
        case 'y':
            use_default_settings = FALSE;
            break;
        case 'n':
        default:
            use_default_settings = TRUE;
            break;
    }
 
    fill_connection_data( (struct sockaddr_in*)&buff, use_default_settings );
 
    print_connecting( (struct sockaddr_in*)&buff );
    if( WINSOCK_SUCCESS != connect( client_socket, (struct sockaddr*)&buff[0], sizeof( struct sockaddr ) ) )
    {
        exit_error_in_function( "connect" );
    }
 
    received_bytes = recv( client_socket, &buff[0], BUFFER_SIZE, 0 );
    printf( "%s", &buff );
 
    while( SOCKET_ERROR != received_bytes
        &&
        0 != received_bytes )
    {
        msg_len = input_message(buff);
        if( 0 == msg_len)
        {
            break;
        }
        send( client_socket, &buff[0], msg_len, 0 );
        received_bytes = recv( client_socket, &buff[0], BUFFER_SIZE, 0 );
        printf( "%s\n", &buff );
    }
 
    closesocket( client_socket );
    system( "pause" );
 
    return 0;
}
