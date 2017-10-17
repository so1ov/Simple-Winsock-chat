#include "stdio.h"
#include "winsock.h"
#include "windows.h"
#include "time.h"
#include "conio.h"

#define BUFFER_SIZE 0x400
#define REQUIRED_WINSOCK_VER 0x0101
#define WINSOCK_SUCCESS 0

const char CONFIG_FILENAME[] = "config.cfg";

HANDLE STDOUT_HANDLE;
const int CONSOLE_WIDTH = 80;
const int CONSOLE_HEIGTH = 23;
const int DISPLAYED_CONNECTIONS_LIMIT = 23 - 6;
const int CONNECTIONS_LIST_FIRST_RAW_OFFSET = 5;
const int CONNECTIONS_LIST_FIRST_COLUMN_OFFSET = 1;
const int CONNECTIONS_LIST_SECOND_COLUMN_OFFSET = 18;
const COORD UPPER_LEFT_CORNER = {0, 0};
const COORD CONNECTIONS_LIST_FIRST_POSITION = {1, 5};
const COORD LOWER_LEFT_CORNER = {0, 23};

const char hello[] =
{
    "Hello from eServer!\n\
     \rType any message to get echo, type ""-close"" to exit\n"
};

const int MAX_CONNECTIONS = 0xFF;

const char DEFAULT_INET_ADDR[] = "192.168.1.100";
const int DEFAULT_LISTENING_PORT = 23;

CRITICAL_SECTION PRINT_CS;

struct
{
    int number_of_clients;
    struct client_info
    {
        int id;
        BOOL is_up;
        HANDLE thread_handle;
        SOCKET client_socket;
        struct in_addr client_address;
    }*client_pool, buff_new_client_info;
}database;

void exit_error_in_function( char function[] )
{
    SetConsoleCursorPosition( STDOUT_HANDLE, UPPER_LEFT_CORNER );
    system( "cls" );
    int error = WSAGetLastError();
    printf( "Function %s ERROR %i\n", function, error );
    system( "pause" );
    exit( error );
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

void print_interface( struct sockaddr* info )
{
    int i;
    EnterCriticalSection(&PRINT_CS);
    SetConsoleCursorPosition( STDOUT_HANDLE, UPPER_LEFT_CORNER );
    printf( "eServer is listening %s:%i...\n",
           inet_ntoa( ((struct sockaddr_in*)info)->sin_addr ), ntohs( ((struct sockaddr_in*)info)->sin_port ) );
    for(i = 0; i < CONSOLE_WIDTH; i++)
    {
        printf( "-" );
    }
    printf( "\n" );
    printf(" Client ID        Client IP\n");
    LeaveCriticalSection(&PRINT_CS);
}

void print_log( char* message, ... )
{
    va_list args;

    EnterCriticalSection(&PRINT_CS);
    SetConsoleCursorPosition( STDOUT_HANDLE, LOWER_LEFT_CORNER );
    va_start( args, message );
    vfprintf( stdout, message, args );
    fprintf( stdout, "\n" );
    LeaveCriticalSection(&PRINT_CS);
}

void clear_connections_list()
{
    int i, j;

    EnterCriticalSection(&PRINT_CS);
    SetConsoleCursorPosition( STDOUT_HANDLE, CONNECTIONS_LIST_FIRST_POSITION );
    for(i = 0; i < DISPLAYED_CONNECTIONS_LIMIT; i++)
    {
        for(j = 0; j < CONSOLE_WIDTH; j++)
        {
            printf(" ");
        }
    }
    LeaveCriticalSection(&PRINT_CS);
}

void connections_list_routine()
{
    int i;
    int displayed_connections;
    COORD buff;

    while( TRUE )
    {
        clear_connections_list();
        EnterCriticalSection(&PRINT_CS);
        SetConsoleCursorPosition( STDOUT_HANDLE, CONNECTIONS_LIST_FIRST_POSITION );
        for(i = 0, displayed_connections = 0; i < MAX_CONNECTIONS; i++)
        {
            if( TRUE == database.client_pool[i].is_up )
            {
                if( displayed_connections <= DISPLAYED_CONNECTIONS_LIMIT )
                {
                    buff.X = CONNECTIONS_LIST_FIRST_COLUMN_OFFSET;
                    buff.Y = CONNECTIONS_LIST_FIRST_RAW_OFFSET + displayed_connections;
                    SetConsoleCursorPosition( STDOUT_HANDLE, buff );
                    printf("%i", i);

                    buff.X = CONNECTIONS_LIST_SECOND_COLUMN_OFFSET;
                    SetConsoleCursorPosition( STDOUT_HANDLE, buff );
                    printf("%s\n", inet_ntoa( database.client_pool[i].client_address ) );

                    displayed_connections += 1;
                }
                else
                {
                    buff.X = CONNECTIONS_LIST_FIRST_COLUMN_OFFSET;
                    buff.Y = CONNECTIONS_LIST_FIRST_RAW_OFFSET + DISPLAYED_CONNECTIONS_LIMIT + 1;
                    printf("\n");
                    printf("<...>\n");
                }
            }

        } /* FOR client counting loop end */
        printf( " - %i active connections -\n", database.number_of_clients );
        LeaveCriticalSection(&PRINT_CS);

        Sleep(2000);
    } /* WHILE main loop end */
}

void client_thread_routine( struct client_info* src )
{
    char msg_buff[ BUFFER_SIZE ];
    int received_bytes;

    send( src->client_socket, hello, sizeof( hello ), 0 );

    received_bytes = recv( src->client_socket, &msg_buff[0], sizeof( msg_buff ), 0 );
    print_log("[LOG] %i bytes received from client %i", received_bytes, src->id);

    while( SOCKET_ERROR != received_bytes
        &&
        0 != received_bytes )
    {
        send( src->client_socket, &msg_buff[0], received_bytes, 0 );
        received_bytes = recv( src->client_socket, &msg_buff[0], sizeof( msg_buff ), 0 );
        print_log( "[LOG] %i bytes received from client %i", received_bytes, src->id );
    }

    closesocket( src->client_socket );
    src->is_up = FALSE;
    database.number_of_clients -= 1;
}

void add_client( SOCKET* src, struct sockaddr* additional_info )
{
    int i;
    BOOL isAdded = FALSE;
    for(i = 0; i < MAX_CONNECTIONS; i++)
    {
        if( FALSE == database.client_pool[i].is_up )
        {
            database.number_of_clients += 1;
            database.client_pool[i].is_up = TRUE;
            database.client_pool[i].id = i;
            memcpy( &(database.client_pool[i].client_socket), src, sizeof( SOCKET ) );
            database.client_pool[i].client_address = ((struct sockaddr_in*)additional_info)->sin_addr;
            database.client_pool[i].thread_handle =
                CreateThread( 0, 0, (LPTHREAD_START_ROUTINE)&client_thread_routine, &(database.client_pool[i]), 0, 0);
            if(0 == database.client_pool[i].thread_handle)
            {
                exit_error_in_function( "CreateThread" );
            }
            isAdded = TRUE;
            break;
        }
    }
    if( FALSE == isAdded )
    {
        print_log( "- Connection with %s:%i rejected due to connection limit -",
                  inet_ntoa( ((struct sockaddr_in*)additional_info)->sin_addr ), ntohs( ((struct sockaddr_in*)additional_info)->sin_port ) );
    }
}

int main()
{
    char buff[ BUFFER_SIZE ];
    SOCKET server_socket;
    SOCKET buff_socket;
    struct sockaddr server_addr_info;
    char choice;
    BOOL use_default_settings;

    InitializeCriticalSection(&PRINT_CS);
    STDOUT_HANDLE = GetStdHandle( STD_OUTPUT_HANDLE );
    database.client_pool = malloc( sizeof( struct client_info ) * MAX_CONNECTIONS );
    memset(database.client_pool, 0, sizeof( struct client_info ) * MAX_CONNECTIONS);
    database.number_of_clients = 0;

    if ( WINSOCK_SUCCESS != WSAStartup( REQUIRED_WINSOCK_VER, (WSADATA*)&buff[0] ) )
    {
        exit_error_in_function( "WSAStartup" );
    }

    server_socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if ( INVALID_SOCKET == server_socket )
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

    fill_connection_data( &server_addr_info, use_default_settings );
    if( SOCKET_ERROR == bind( server_socket, (struct sockaddr*)&server_addr_info, sizeof(server_addr_info) ) )
    {
        exit_error_in_function( "bind" );
    }

    if( SOCKET_ERROR == listen( server_socket, MAX_CONNECTIONS ) )
    {
        exit_error_in_function( "listen" );
    }

    print_interface( &server_addr_info );
    CreateThread( 0, 0, (LPTHREAD_START_ROUTINE)&connections_list_routine, 0, 0, 0);
    while( TRUE )
    {
        memset(buff, 0, sizeof( struct sockaddr ));
        buff_socket = accept( server_socket, (struct sockaddr*)&buff, 0 );
        if( INVALID_SOCKET == buff_socket )
        {
            exit_error_in_function( "accept" );
        }
        else
        {
            add_client( &buff_socket, (struct sockaddr*)&buff );
        }
    }

    return 0;
}

