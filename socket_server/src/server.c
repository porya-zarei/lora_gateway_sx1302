// Server side C/C++ program to demonstrate Socket
// programming
// #include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h> /* socket specific definitions */
#include <netinet/in.h> /* INET constants and stuff */
#include <arpa/inet.h>  /* IP address conversion stuff */
#include <netdb.h>      /* gai_strerror */
#include <unistd.h>
#include <sys/types.h>

#include "parson.h"
#include "base64.h"

#define PORT 8080

// uint8_t data_send[] =
//     {
//         2, token_h, token_l, 3, 'M', 'A', 'C', '*', 'M', 'A', 'C', '#',
//         /* JSON string */
//         '{', '"', 't', 'x', 'p', 'k', '"', ':', ' ', '{', '"', 'i', 'm', 'm', 'e', '"',
//         ':', ' ', 't', 'r', 'u', 'e', ',', '"', 'f', 'r', 'e', 'q', '"', ':', ' ', '4', '3', '3',
//         '.', '6', ',', '"', 'r', 'f', 'c', 'h', '"', ':', ' ', '0', ',', '"', 'm', 'o',
//         'd', 'u', '"', ':', ' ', '"', 'L', 'O', 'R', 'A', '"', ',', '"', 'd', 'a', 't', 'r',
//         '"', ':', ' ', '"', 'S', 'F', '0', '7', 'B', 'W', '1', '2', '5', '"', ',', '"', 'c',
//         'o', 'd', 'r', '"', ':', ' ', '"', '4', '/', '5', '"', ',', '"', 'i', 'p', 'o', 'l',
//         '"', ':', ' ', 'f', 'a', 'l', 's', 'e', ',', '"', 's', 'i', 'z', 'e', '"', ':', ' ', '1',
//         '6', ',', '"', 'd', 'a', 't', 'a', '"', ':', ' ', '"', 'S', 'G', 'l', 'Q', 'b', '3', 'J',
//         '5', 'Y', 'U', 'h', 'v', 'd', '0', 'F', 'y', 'Z', 'V', 'l', 'v', 'd', 'Q', '=', '=', '"', ',', '"', 'n', 'c', 'r', 'c', '"', ':', ' ', 'f', 'a', 'l', 's', 'e', ',', '"',
//         'n', 'h', 'd', 'r', '"', ':', ' ', 'f', 'a', 'l', 's', 'e', ',', '"', 'p', 'r', 'e', 'a',
//         '"', ':', ' ', '8', ',', '"', 'p', 'o', 'w', 'e', '"', ':', ' ', '2', '7', '}', '}'};

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    // clear address structure
    bzero((char *)&serv_addr, sizeof(serv_addr));

    /* setup the host_addr structure for use in bind call */
    // server byte order
    serv_addr.sin_family = AF_INET; //AF_INET;

    // automatically be filled with current host's IP address
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // convert short integer value for port must be converted into network byte order
    serv_addr.sin_port = htons(PORT);

    // bind(int fd, struct sockaddr *local_addr, socklen_t addr_length)
    // bind() passes file descriptor, the address structure,
    // and the length of the address structure
    // This bind() call will bind  the socket to the current IP address on port, portno
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        error("ERROR on binding");
    }

    printf("server start listening on port (%d) ...\r\n", PORT);

    uint8_t token_h = (uint8_t)rand(); /* random token */
    uint8_t token_l = (uint8_t)rand(); /* random token */

    uint8_t data_send[12] = {2, token_h, token_l, 3, 'M', 'A', 'C', '*', 'M', 'A', 'C', '#'};

    JSON_Value *root;
    const char *json_file_name = "txpkt.json";
    const char *string_value;
    char *result;

    // Load and parse the JSON file
    root = json_parse_file_with_comments(json_file_name);
    if (root == NULL)
    {
        printf("Failed to parse JSON file.\n");
        return 1;
    }

    string_value = json_serialize_to_string(root);
    size_t buf_size_bytes = json_serialization_size(root);
    printf("INFO: string value=> %s, size => %d\r\n", string_value, buf_size_bytes);
    result = malloc(buf_size_bytes + sizeof(data_send));
    strcat(result, (char *)data_send);
    strcat(result, string_value);
    // result[12 + buf_size_bytes + 1] = '\0';
    printf("INFO: text => %s, size:%d\r\n", result, strlen(result));
    // This listen() call tells the socket to listen to the incoming connections.
    // The listen() function places all incoming connection into a backlog queue
    // until accept() call accepts the connection.
    // Here, we set the maximum size for the backlog queue to 5.
    listen(sockfd, 1);

    // The accept() call actually accepts an incoming connection
    clilen = sizeof(cli_addr);

    // This accept() function will write the connecting client's address info
    // into the the address structure and the size of that structure is clilen.
    // The accept() returns a new socket file descriptor for the accepted connection.
    // So, the original socket file descriptor can continue to be used
    // for accepting new connections while the new socker file descriptor is used for
    // communicating with the connected client.
    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

    if (newsockfd < 0)
        error("ERROR on accept");
    uint8_t rand_num = 0;

    printf("server: got connection from %s port %d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
    printf("server start loop");

    while (1)
    {
        n = read(newsockfd, buffer, 255);
        if (n < 0)
            error("ERROR reading from socket");
        else
        {
            printf("Here is the message: %s\n", (char *)(buffer + 12));
            // uint8_t[Protocol Version (2)], uint8_t[Token High], uint8_t[Token Low], uint32_t[MAC High], uint32_t[MAC Low], + [uint8_t][Data]
            rand_num = (uint8_t)(rand() % 7) + 6;
            data_send[1] = (uint8_t)rand();
            data_send[2] = (uint8_t)rand();
            send(newsockfd, data_send, sizeof(data_send), 0);
        }
    }
    close(newsockfd);
    close(sockfd);
    return 0;
}
