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
#include <zlib.h>

/** @brief Decrompress data compressed with gzip
 *  @param inBuffer ingoing data
 *  @param inLength size of ingoing data
 *  @param outBuffer pointer to a variable which will get the result
 *  @return Size of the output buffer. 0 in case of error
 */
size_t DecompressGzipBuffer(void *inData, size_t inLength, void** outBuffer) {
    if (inLength == 0) return 0;

    size_t bufferLength = inLength + inLength/2;
    size_t buffer = malloc(bufferLength);
    if (!buffer) return 0;

    //NSMutableData *decompressed = [NSMutableData dataWithLength: ];
    BOOL done = false; int status;

    z_stream strm; strm.next_in = (Bytef *)inData; strm.avail_in = inLength;
    strm.total_out = 0; strm.zalloc = Z_NULL; strm.zfree = Z_NULL;

    if (inflateInit2(&strm, (15+32)) != Z_OK)
        return nil;

    while (!done) { // Make sure we have enough room and reset the lengths.
        if (strm.total_out >= bufferLength) {
            bufferLength += inLength/2;
            buffer = realloc(bufferLength, bufferLength);
            if (!buffer) return 0;
        }
        strm.next_out = buffer + strm.total_out;
        strm.avail_out = inLength - strm.total_out;

        // Inflate another chunk.
        status = inflate (&strm, Z_SYNC_FLUSH);
        if (status == Z_STREAM_END)
            done = true;
        else if (status != Z_OK)
            break;
    }
    if (inflateEnd (&strm) != Z_OK) return 0;

        // Set real length.
    if (done) {
        *outBuffer = buffer;
        return strm.total_out;
    } else return 0;
}

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
ssize_t ReceiveResponse(int fd, char **responseData) {
    ssize_t bufferLen = 4096, bytesRcvd = 0, parserOffset = 0, headerLength = 0, contentLength = 0;
    bool usesGzip = false, headerReceived = false;
    int status = 0;

	char *buffer = (char *) malloc(bufferLen);
    if (!buffer) {
        printf("error malloc");
        return -1;
    }

	while(!headerReceived || bytesRcvd < contentLength + headerLength) {
		ssize_t status = recv(fd, (uint8_t*)buffer + bytesRcvd, bufferLen - bytesRcvd, 0);
		if(status == -1) {// print user-friendly message on error
			printf("Error recv");
			goto error_cleanup;
		} else if(status == 0) { // print error message on connection close
			printf("Connection closed by the server.\n");
			break;
		}
		bytesRcvd += status;
		if (bufferLen - bytesRcvd < 512) {// Pretty arbitrary
		    bufferLen += bytesRcvd/2;
		    buffer = realloc(buffer, bufferLen);
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
                if (!pch) goto error_cleanup;// should not happen

                // atoi should ignore whitespaces and characters after the status code
                status = atoi(pch + sizeof(http)+1);
                printf("Received status %d\n", status);
                if (status != 200) goto error_cleanup;

            } else if (lineEnd + 3 < responseRecv// Check if this is the end of the header
            && response[lineEnd + 2] == '\r' && response[lineEnd + 3] == '\n') {
                headerLength = lineEnd + 3 + 1;// lineEnd is a 0 based index
                headerReceived = true;
            } else {
                // Looking for something like "Content-Length: 1354"
                char *pch = strchr(buffer + parserOffset, ':');
                if (!pch) break;// TODO should not happen
                *pch = '\0';// Terminate so we can use strcasecmp

                // TODO Could there be leading whitespaces?
                if (strcasecmp(buffer + parserOffset, "Content-Length") == 0) {
                    contentLength = atol(pch + 1);// should work?
                    if (bufferLen - contentLength < 0) {
                        bufferLen += contentLength - bufferLen;
                        buffer = realloc(buffer, bufferLen);
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

    // This must hold after every request
	if (headerReceived) {
	    if (bytesRcvd > headerLength) {// Got a body
	        if (usesGzip) {
        	    size_t responseLength = DecompressGzipBuffer(buffer + headerLength, bytesRcvd - headerLength, &responseData);
        	    free(buffer);
        	    return responseLength;
        	} else {
        	    /*buffer[bytesRcvd-1] = '\0';
                            printf("%s\n", (const char*)(buffer + headerLength));*/

                // Todo I can't return the offset
                *responseData = buffer + headerLength;
                return responseRecv - headerLength;
        	}
        }
        return 0;// No body received
	}

    // Sometimes life just doesn't work out
error_cleanup:
    printf("Error cleanup");
    free(buffer); // free memory
    return -1;
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
