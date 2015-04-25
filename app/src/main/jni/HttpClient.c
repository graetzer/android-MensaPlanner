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
#import <zlib.h>

/* // Gzip deflate, maybe look for something in c
    z_stream strm;

    strm.zalloc = Z_NULL; strm.zfree = Z_NULL;
    strm.opaque = Z_NULL; strm.total_out = 0;
    strm.next_in=(Bytef *)[self bytes]; strm.avail_in = [self length];

    // Compresssion Levels: // Z_NO_COMPRESSION // Z_BEST_SPEED // Z_BEST_COMPRESSION // Z_DEFAULT_COMPRESSION

    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, (15+16), 8, Z_DEFAULT_STRATEGY) != Z_OK) return nil;

    NSMutableData *compressed = [NSMutableData dataWithLength:16384]; // 16K chunks for expansion

    do {
        if (strm.total_out >= [compressed length])
            [compressed increaseLengthBy: 16384];

        strm.next_out = [compressed mutableBytes] + strm.total_out;
        strm.avail_out = [compressed length] - strm.total_out;

        deflate(&strm, Z_FINISH);

    } while (strm.avail_out == 0);

    deflateEnd(&strm);

    [compressed setLength: strm.total_out];
    return [NSData dataWithData:compressed]; }

*/

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

/**
* Find a line ending CRLF. Returns index of CR
*/
ssize_t FindLineEnding(void* data, size_t len) {
    size_t i = 0;
    while(i+1 < len) {
        if (data[i] == '\r' && data[i+1] == '\n') return i;
        i+=2;
    }
    return -1;
}

/** @brief Receives a http response using the given socket and prints it to stdout
 *  @param socket descriptor
 *  @param responseBodyData Pointer to the variable which will get the response buffer. Needs to be free()'d afterwards
 *  @return 0 on success, -1 on failure
 */
ssize_t ReceiveResponse(int fd, void **responseData) {

    ssize_t bufferLen = 4096, bytesRcvd = 0, parserOffset = 0, headerLength = 0;
	char *buffer = (char *) malloc(bufferLen);
    if (!buffer) {
        printf("error malloc");
        return -1;
    }
    bool usesGzip = false, headerReceived = false;
    int status = 0;

	while(true) {// there are still bytes to receive
		ssize_t status = recv(fd, (uint8_t*)buffer + bytesRcvd, bufferLen - bytesRcvd, 0);
		if(status == -1) {// print user-friendly message on error
			free(buffer); // free memory
			printf("recv");
			return -1;
		} else if(status == 0) { // print error message on connection close
			//free(response); // free memory
			printf("Connection closed by the server.\n");
            // TODO
			break;
		}
		bytesRcvd += status;
		if (bufferLen - bytesRcvd < 512) {// Pretty arbitrary
		    buffer = realloc(buffer, bufferLen + bytesRcvd/2);
		}

        // Try to parse the header, pretty ugly
        if (!headerReceived && status > 0) {
            ssize_t lineEnd = FindLineEnding(buffer + parserOffset, bytesRcvd - parserOffset);
            if (lineEnd == -1) continue;

            buffer[lineEnd] = '\0';// Artificially terminate the line
            if (status == 0) {// Should not be 0 after the status line was received
                // Looking for: "HTTP/1.1 200 OK"
                char *http = "HTTP/1.";
                char *pch = strstr(buffer, http);// pointer to
                if (!pch) break;// TODO should not happen

                // atoi should ignore whitespaces and characters after
                status = atoi(pch + sizeof(http)+1);
                printf("Received status %d\n", status);
                if (status != 200) {
                    free(buffer); // free memory
                    return -1;
                }

            } else if (lineEnd + 4 < responseRecv// Check if this is the end of the header
            && response[lineEnd + 2] == '\r' && response[lineEnd + 3] == '\n') {
                headerLength = lineEnd + 4;
                headerReceived = true;
            } else {
                // Looking for something like "Content-Length: 1354"
                char *pch = strchr(buffer + parserOffset, ':');
                if (!pch) break;// TODO should not happen
                *pch = '\0';// Terminate so we can use strcasecmp

                // TODO Could there be leading whitespaces?
                if (strcasecmp(buffer + parserOffset, "Content-Length") == 0) {
                    int bodyLen = atoi(pch + 1);// should work?
                    if (bufferLen - bodyLen < 0) {
                        buffer = realloc(buffer, bufferLen*2  - bodyLen);
                    }
                } else if (strcasecmp(buffer + parserOffset, "Content-Encoding") == 0) {
                    // Looking for "Content-Encoding: gzip"
                    if (strstr(pch + 1, "gzip") != null) {
                        usesGzip = true;
                    }
                }
            }

            parserOffset = lineEnd+2;
        }
	}

	if (usesGzip) {
	    // TODO
	}

    if (responseReceived > 0 && headerReceived) {
        buffer[responseRecv]
        printf("%s\n", (const char*)(response + headerLength));
    }
    if (headerReceived && responseData) {
        *responseData = response + headerLength;
    	return responseRecv - headerLength;
    }
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
