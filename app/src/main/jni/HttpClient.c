#include "de_rwth_aachen_comsys_assignment2_data_MealPlan.h"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <zlib.h>

#include <android/log.h>
#define debugLog(...) __android_log_print(ANDROID_LOG_DEBUG, "HttpClient", __VA_ARGS__)
//#define debugLog(...) printf(__VA_ARGS__)

typedef struct {
	long status; /// entire request, free this pointer
	bool usesGzip;
	size_t headerLength, contentLength;
} http_header;

typedef struct {
    http_header header;
	uint8_t *buffer; /// entire request, free this pointer
	uint8_t *content; /// pointer to content
} http_response;

/** @brief Decrompress data compressed with gzip
 *  @param inBuffer ingoing data
 *  @param inLength size of ingoing data
 *  @param outBuffer pointer to a variable which will get the result
 *  @return Size of the output buffer. 0 in case of error
 */
size_t DecompressGzipBuffer(uint8_t *inData, size_t inLength, uint8_t** outBuffer) {
    if (inLength == 0) return 0;

    bool done = false; int status;
    size_t bufferLength = inLength + inLength/2;
    uint8_t* buffer = (uint8_t *)malloc(bufferLength);
    if (!buffer) return 0;

    z_stream strm; strm.next_in = (Bytef *)inData; strm.avail_in = inLength;
    strm.total_out = 0; strm.zalloc = Z_NULL; strm.zfree = Z_NULL;

    if (inflateInit2(&strm, (15+32)) != Z_OK) {
        debugLog("DecompressGzipBuffer: inflateInit2 is not ok\n");
        return 0;
    }

    while (!done) { // Make sure we have enough room and reset the lengths.
        if (strm.total_out >= bufferLength) {
            debugLog("DecompressGzipBuffer: increasing buffer size\n");
            bufferLength += inLength/2;
            buffer = (uint8_t *) realloc(buffer, bufferLength);
            if (!buffer) return 0;
        }
        strm.next_out = buffer + strm.total_out;
        strm.avail_out = bufferLength - strm.total_out;

        // Inflate another chunk.
        status = inflate (&strm, Z_SYNC_FLUSH);
        if (status == Z_STREAM_END)
            done = true;
        else if (status != Z_OK) {
            debugLog("DecompressGzipBuffer: inflate is not ok. status = %d\n", status);
            break;
        }
    }
    if (inflateEnd (&strm) != Z_OK) {
        debugLog("DecompressGzipBuffer: inflateEnd is not ok\n");
        return 0;
    }

        // Set real length.
    if (done) {
        *outBuffer = buffer;
        return strm.total_out;
    } else {
        debugLog("DecompressGzipBuffer: Error done is false\n");
        return 0;
    }
}

/** @brief Resolves the given hostname/port combination
 *  @param server hostname to resolve
 *  @param port port to resolve
 *  @return resolved address(es)
 */
struct addrinfo *ResolveHost(const char *server, const char *port) {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET; // ipv4 please
	hints.ai_socktype = SOCK_STREAM; // request TCP

	struct addrinfo *result = 0;
	int errorCode = getaddrinfo(server, port, &hints, &result); // resolve hostname

	if(errorCode != 0) { // print user-friendly message on error
		debugLog("Name resolution failed: %s\n", gai_strerror(errorCode));
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
		debugLog("socket");
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
		debugLog("setsockopt");
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
ssize_t ConnectAndSendGET(int fd, struct addrinfo *addr, const char *host, const char *path) {
	char *request;// asprintf should be available everywhere, alternatively use snprintf
	int requestLen = asprintf(&request,
	"GET %s HTTP/1.1\r\n"
	"Host: %s\r\n"
	"Accept: */*\r\n"
	"Connection: close\r\n"// we don't do keep-alive
	//"TE: identity\r\n"
	//"Accept-Encoding: gzip\r\n"
	//"TE: trailers;q=0\r\n"
	"\r\n",// empty line marks end
	path, host);
	if(requestLen == -1) { // print user-friendly message on error
    	debugLog("error asprintf\n");
    	return -1;
    }
    //debugLog("Sending Message:\n%s", request);

	if (connect(fd, addr->ai_addr, addr->ai_addrlen) == -1) {
		debugLog("error connect: %s\n", strerror(errno));
		return -1;
	}
	ssize_t result = 0, total = 0;
	while (total < requestLen) {
	    result = send(fd, request+total, requestLen-total, 0);
	    if (result == -1) break;
	    total += result;
	}
	if(result == -1) debugLog("Error send: %s\n", strerror(errno));

	free(request);
	return result;
}

/**
 * Expects the buffer to have at least one '\0' at the end.
 */
bool ParseHeader(uint8_t *buffer, uint8_t bufferLength, http_header* header) {

    // Wait till we have the complete header
    char *pch;
    if ((pch = strstr(buffer, "\r\n\r\n")) == 0) return false;
    header->headerLength += (size_t)(pch+4 - (char*)buffer);//skip the last CR|LF|CR|LF
    debugLog("Found end of header\n");

    char* parserPointer = buffer;
    while (parserPointer < (char*)buffer + header->headerLength) {
        char *lineBegin = parserPointer;
        char *lineEnd = strstr(lineBegin, "\r\n");// Look for CRLF
        if (!lineEnd) break;// Shouldn't happen
        parserPointer = lineEnd + 2;//skip CRLF

        const char* http = "HTTP/1.";
        if ((pch = strstr(lineBegin, http)) != 0) {// Looking for: "HTTP/1.1 200 OK"
            header->status = atol(pch + strlen(http)+2);
            debugLog("Received status code %ld", header->status);

        } else {
            // Artificially terminate the line so we use strstr, strchr and printf
            *lineEnd = '\0';
            debugLog("Header '%s'", lineBegin);

            // Looking for a header like "Content-Length: 1354"
            if ((pch = strchr(lineBegin, ':')) == 0) continue;

            *pch = '\0';// Terminate so we can use strcasecmp
            // TODO Could there be leading whitespaces? Seems to work anyway
            if (strcasecmp(lineBegin, "Content-Length") == 0) {
                header->contentLength = atol(pch + 1);
                debugLog("Received Content-Length: %zu\n", header->contentLength);

            } else if (strcasecmp(lineBegin, "Content-Encoding") == 0) {
                // Looking for "Content-Encoding: gzip"
                if (strstr(pch + 1, "gzip")) {
                    debugLog("Using gzip");
                    header->usesGzip = true;
                }
            }
        }
    }
    return true;
}

/** @brief Receives a http response using the given socket and prints it to stdout
 *  @param socket descriptor
 *  @param responseBodyData Pointer to the variable which will get the response buffer. Needs to be free()'d afterwards
 *  @return 0 on success, -1 on failure
 */
bool ReceiveResponse(int fd, http_response *response) {
    size_t bufferLen = 4096, bytesRcvd = 0;
    bool headerReceived = false;
    http_header header;
    memset(&header, 0, sizeof(http_header));

	uint8_t *buffer = (uint8_t *) malloc(bufferLen);
    if (!buffer) {
        debugLog("error malloc");
        return -1;
    }

	while(!headerReceived
	      || (header.contentLength != 0 && bytesRcvd <= header.contentLength + header.headerLength)) {

		ssize_t byteCount = recv(fd, buffer + bytesRcvd, bufferLen - bytesRcvd, 0);
		if(byteCount == -1) {// print user-friendly message on error
			debugLog("Error recv");
			goto error_cleanup;
		} else if(byteCount == 0) { // print error message on connection close
			debugLog("Connection closed by the server.\n");
			break;
		}
		// We are gonna fill up the buffer
		bytesRcvd += byteCount;
		if (bufferLen - bytesRcvd < 512) {// Pretty arbitrary
		    bufferLen += bytesRcvd/2;
		    buffer = (uint8_t *) realloc(buffer, bufferLen);
		    if (!buffer) goto error_cleanup;
		}

        // Try to parse the header, pretty ugly
        if (!headerReceived) {
            // The buffer should be always bigger so we can do this without worries
            buffer[bytesRcvd+1] = '\0';
            headerReceived = ParseHeader(buffer, bytesRcvd+1, &header);
            // If contentLength is suddenly greater than 0
            if (bufferLen < header.contentLength+bytesRcvd) {
                bufferLen += header.contentLength + bytesRcvd - bufferLen;
                buffer = (uint8_t *) realloc(buffer, bufferLen);
                if (!buffer) goto error_cleanup;
            }
        }
	}

    debugLog("Bytes Received: %zu\nHeader length %zu\n", bytesRcvd, header.headerLength);
    // This must hold after every request
	if (headerReceived) {
	    if (bytesRcvd > (size_t)header.headerLength) {// Got a body
	        if (header.contentLength != bytesRcvd - header.headerLength) {
                debugLog("Invalid content length\n");
                goto error_cleanup;
            }

	        if (header.usesGzip) {
	            debugLog("Try  to decompress body");
        	    header.contentLength = DecompressGzipBuffer(buffer + header.headerLength,
        	    bytesRcvd - header.headerLength,
        	    &(response->content));
        	    debugLog("New content length %zu", header.contentLength);

        	    free(buffer);
        	    response->buffer = response->content;
        	} else {
        	    response->buffer = buffer;
        	    response->content = buffer + header.headerLength;
        	}
        }
        response->header = header;
        return true;// All cool
	}
	debugLog("It seems there was no complete header\n");

    // Sometimes life just doesn't work out
error_cleanup:
    debugLog("Error cleanup\n");
    free(buffer);
    return false;
}

/*
 * Class:     de_rwth_aachen_comsys_assignment2_data_MealPlan
 * Method:    requestUrl
 * Signature: (Ljava/lang/String;Ljava/lang/String;)[B
 */
JNIEXPORT jbyteArray JNICALL Java_de_rwth_1aachen_comsys_assignment2_data_MealPlan_requestUrl (JNIEnv *env, jobject object, jstring serverhost, jstring path) {
    const char *cServerHost = (*env)->GetStringUTFChars(env, serverhost, 0);
    const char *cPath = (*env)->GetStringUTFChars(env, path, 0);
    //*/
    //Testing http://www.studentenwerk-aachen.de/speiseplaene/academica-w.html
    //const char *cServerHost = "www.studentenwerk-aachen.de";
    //const char *cPath = "/speiseplaene/academica-w.html";

    struct addrinfo *host = ResolveHost(cServerHost, "80"); // resolve hostname and port
    if(!host) return NULL;// exit if hostname could not be resolved


    int fd = CreateSocketWithOptions(host); // create socket
    if(fd == -1) { // exit if the socket could not be created
        freeaddrinfo(host); // free addrinfo(s)
        debugLog("Error CreateSocketWithOptions");
        return NULL;
    }

    if(ConnectAndSendGET(fd, host, cServerHost, cPath) == -1) { // connect & send request
        close(fd); // close socket
        freeaddrinfo(host); // free addrinfo(s)
        debugLog("Error ConnectAndSendGET");
        return NULL;
    }

    http_response response;
    memset(&response, 0, sizeof(http_response));
    if(ReceiveResponse(fd, &response) == false) { // receive & print response
        close(fd); // close socket
        freeaddrinfo(host); // free addrinfo(s)
        debugLog("Error ReceiveResponse");
        return NULL;
    }

    /*response.content[response.contentLength-1] = '\0';
    debugLog("Body:\n%s\n", (char *)response.content);*/

    // Final result array
    jbyteArray arr = (*env)->NewByteArray(env, response.header.contentLength);
    (*env)->SetByteArrayRegion(env,arr,0,response.header.contentLength, (jbyte*)response.content);

    //free(response.buffer);
    close(fd); // close socket
    freeaddrinfo(host); // free addrinfo(s)

    // Releasing allocated c strings
    (*env)->ReleaseStringUTFChars(env, serverhost, cServerHost);
    (*env)->ReleaseStringUTFChars(env, path, cPath);
    return arr;
}
