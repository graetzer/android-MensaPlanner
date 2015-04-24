#include "de_rwth_aachen_comsys_assignment2_data_MealPlan.h"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdout.h>
#include <unistd.h>
#include <string.h>


enum PacketTypes {
	JOKER_REQUEST_TYPE = 1, /// packet type for joke-requests (see joker_request)
	JOKER_RESPONSE_TYPE = 2 /// packet type for joke-responses (see joker_response)
};

const uint32_t MAX_JOKE_LENGTH = 1024; /// maximum accepted length of jokes in joker_response packets

typedef struct {
	uint8_t type; /// first byte in every joke packet, identifies the packet type (see PacketTypes)
} __attribute__ ((__packed__)) joker_header;

typedef struct {
	joker_header header; /// packet header
	uint8_t len_first_name; /// length of the first name
	uint8_t len_last_name; /// length of the last name
} __attribute__ ((__packed__)) joker_request;

typedef struct {
	joker_header header; /// packet header
	uint32_t len_joke; // length of the joke
} __attribute__ ((__packed__)) joker_response;

/** @brief Resolves the given hostname/port combination
 *  @param server hostname to resolve
 *  @param port port to resolve
 *  @return resolved address(es)
 */
struct addrinfo *ResolveHost(const char *server, const char *port) {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC; // we don't care about ipv4 or ipv6
	hints.ai_socktype = SOCK_STREAM; // request TCP

	struct addrinfo *result = 0;
	int errorCode = getaddrinfo(server, port, &hints, &result); // resolve hostname

	if(errorCode != 0) { // print user-friendly message on error
		printf("Name resolution failed: %s\n", gai_strerror(errorCode));
		return 0;
	}
	return result;
}

/** @brief Creates a socket and sets its receive timeout
 *  @param host addrinfo struct pointer returned by ResolveHost/getaddrinfo
 *  @return Returns socket descriptor on success, -1 on failure
 */
int CreateSocketWithOptions(struct addrinfo *host) {
	int fd = socket(host->ai_family, host->ai_socktype, host->ai_protocol); // create socket using provided parameters
	if(fd == -1) { // print user-friendly message on error
		printf("socket");
		return -1;
	}

	struct timeval delay;
	memset(&delay, 0, sizeof(struct timeval));
	delay.tv_sec = 30; // 3 seconds timeout
	delay.tv_usec = 0;

	socklen_t delayLen = sizeof(delay);
	int status = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &delay, delayLen); // apply timeout to socket
	if(status == -1) { // print user-friendly message on error
		close(fd);
		printf("setsockopt");
		return -1;
	}

	return fd;
}

/** @brief Connects to the given host with the given socket and requests a resource
 *  @param fd socket descriptor
 *  @param host target host info
 *  @param host Server host
 *  @param path Resource path + query
 *  @return Number of bytes sent, -1 on failure
 */
ssize_t ConnectAndSendGET(int fd, struct addrinfo *host, const char *host, const char *path) {
	char *request;// asprintf should be available everywhere, alternatively use snprintf
	int requestLen = asprintf(&buffer,
	"GET %s HTTP/1.1\r\n"
	"Host: %s\r\n"
	"User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.10; rv:37.0) Gecko/20100101 Firefox/37.0\r\n"// Just because
	"Connection: close\r\n"// we don't do keep-alive
	"\r\n",// empty line marks end
	path, host);//Accept-Encoding: gzip, deflate
	if(requestLen == -1) { // print user-friendly message on error
    	printf("error asprintf");
    	return -1;
    }

	// MSG_FASTOPEN doesn't work on BSD or most other systems
#ifdef MSG_FASTOPEN
	ssize_t result = sendto(fd, request, requestLen, MSG_FASTOPEN, host->ai_addr, host->ai_addrlen); // send data with TCP fast open
#else
	if (connect(fd, host->ai_addr, host->ai_addrlen) == -1) {
		printf("error connect");
		return -1;
	}
	ssize_t result = send(fd, request, requestLen, 0);
#endif

	free(request); // free allocated memory
	if(result == -1) { // print user-friendly message on error
		printf("error sendto");
		return -1;
	}
	return result;
}

/** @brief Receives a http response using the given socket and prints it to stdout
 *  @param socket descriptor
 *  @param response Pointer to the variable which will get the response buffer. Needs to be free()'d afterwards
 *  @return 0 on success, -1 on failure
 */
ssize_t ReceiveResponse(int fd, void **response) {
	const size_t responseLen = sizeof(joker_response); // response length excluding joke
	size_t allowedLen = responseLen, // maximum number bytes to receive
		   receivedLen = 0; // number of bytes received

	void *response = malloc(responseLen + MAX_JOKE_LENGTH); // allocate memory for response

	while(receivedLen < allowedLen) {// there are still bytes to receive
		ssize_t status = recv(fd, (uint8_t*)response + receivedLen, allowedLen - receivedLen, 0);/

		if(status == -1) {// print user-friendly message on error
			free(response); // free memory
			printf("recvfrom");
			return -1;
		}
		else if(status == 0) { // print error message on connection close
			free(response); // free memory
			printf("Connection closed by the server.\n");
			return -1;
		}

		receivedLen += status; // status = received number of bytes
		if(receivedLen >= responseLen) {
			uint32_t jokeLen = ntohl(response->len_joke);
			if(jokeLen >= MAX_JOKE_LENGTH) {
				free(response); // free memory
				printf("The response contains a too long joke. Not funny.\n");
				return -1;
			}

			allowedLen = responseLen + jokeLen;
		}
		if(receivedLen > 0 && response->header.type != JOKER_RESPONSE_TYPE) {
			free(response); // free memory
			printf("Unknown packet type in response.\n");
			return -1;
		}
	}

	// never writes out of bounds because we abort if jokeLen == MAX_JOKE_LENGTH
	*((uint8_t*)response + allowedLen) = 0; // null-terminate response

	printf("%s\n", (const char*)response + responseLen); // print joke to stdout

	free(response); // free memory
	return 0;
}

/*
 * Class:     de_rwth_aachen_comsys_assignment2_data_MealPlan
 * Method:    requestUrl
 * Signature: (Ljava/lang/String;Ljava/lang/String;)[B
 */
JNIEXPORT jbyteArray JNICALL Java_de_rwth_1aachen_comsys_assignment2_data_MealPlan_requestUrl
  (JNIEnv *env, jobject object, jstring serverhost, jstring path) {
    const char *cServerHost = (*env)->GetStringUTFChars(env, serverhost, 0);
    const char *cPath = (*env)->GetStringUTFChars(env, path, 0);

    struct addrinfo *host = ResolveHost(cServerHost, 80); // resolve hostname and port

    if(!host) // exit if hostname could not be resolved
        return -1;

    int fd = CreateSocketWithOptions(host); // create socket
    if(fd == -1) { // exit if the socket could not be created
        freeaddrinfo(host); // free addrinfo(s)
        return -2;
    }

    if(ConnectAndSendData(fd, host, cServerHost, cPath) == -1) { // connect & send request
        close(fd); // close socket
        freeaddrinfo(host); // free addrinfo(s)
        return -3;
    }

    if(ReceiveResponse(fd) == -1) { // receive & print response
        close(fd); // close socket
        freeaddrinfo(host); // free addrinfo(s)
        return -4;
    }

    close(fd); // close socket
    freeaddrinfo(host); // free addrinfo(s)

    // Releasing allocated c strings
    (*env)->ReleaseStringUTFChars(env, serverhost, cServerHost);
    (*env)->ReleaseStringUTFChars(env, path, cPath);
    //(*env)->NewStringUTF(env, "Hello");
  }
