#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#include <applibs/log.h>
#include <applibs/networking.h>

#include <curl/curl.h>
#include <curl/easy.h>

const char Text[] = { 'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd' };
const char *FileURI = "https://<your-stroage-account>.blob.core.windows.net/img/hello.txt";
const char *SASToken = "Create a Service SAS with Create and Write permission and paste Query String here";

static volatile sig_atomic_t terminationRequired = false;

static void TerminationHandler(int signalNumber)
{
	terminationRequired = true;
}

static void LogCurlError(const char* message, int curlErrCode)
{
	Log_Debug(message);
	Log_Debug(" (curl err=%d, '%s')\n", curlErrCode, curl_easy_strerror(curlErrCode));
}

static size_t read_callback(char* buffer, size_t size, size_t nitems, void* userdata)
{
	memcpy(buffer, Text, sizeof(Text));
	return sizeof(Text);
}

static void UploadFileToAzureBlob(void)
{
	CURL* curlHandle = NULL;
	CURLcode res = CURLE_OK;
	struct curl_slist* list = NULL;

	if ((res = curl_global_init(CURL_GLOBAL_ALL)) != CURLE_OK) {
		LogCurlError("curl_global_init", res);
		goto exitLabel;
	}

	char *sasurl = calloc(strlen(FileURI) + strlen(SASToken) + sizeof('\0'), sizeof(char));
	(void)strcat(strcat(sasurl, FileURI), SASToken);

    if ((curlHandle = curl_easy_init()) == NULL) {
        Log_Debug("curl_easy_init() failed\r\n");
        goto cleanupLabel;
    }

	if ((res = curl_easy_setopt(curlHandle, CURLOPT_URL, sasurl)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_URL", res);
		goto cleanupLabel;
	}

	list = curl_slist_append(list, "x-ms-blob-type:BlockBlob");
	if ((res = curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, list)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_URL", res);
		goto cleanupLabel;
	}

	if ((res = curl_easy_setopt(curlHandle, CURLOPT_UPLOAD, 1)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_UPLOAD", res);
		goto cleanupLabel;
	}
	if ((res = curl_easy_setopt(curlHandle, CURLOPT_INFILESIZE, sizeof(Text))) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_INFILESIZE", res);
		goto cleanupLabel;
	}

	if ((res = curl_easy_setopt(curlHandle, CURLOPT_READFUNCTION, read_callback)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_READFUNCTION", res);
		goto cleanupLabel;
	}

	if ((res = curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 1L)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_VERBOSE", res);
		goto cleanupLabel;
	}

	if ((res = curl_easy_perform(curlHandle)) != CURLE_OK) {
		LogCurlError("curl_easy_perform", res);
	}

cleanupLabel:
	free(sasurl);
	// Clean up sample's cURL resources.
	curl_easy_cleanup(curlHandle);
	// Clean up cURL library's resources.
	curl_global_cleanup();

exitLabel:
	return;
}

int main(void)
{
	Log_Debug("Example to upload a file to Azure Blob Service by using libcurl and REST API\r\n");

	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = TerminationHandler;
	sigaction(SIGTERM, &action, NULL);

	bool isNetworkingReady = false;
	while ((Networking_IsNetworkingReady(&isNetworkingReady) < 0) || !isNetworkingReady) {
		Log_Debug("\nNot doing download because network is not up, try again\r\n");
	}

	UploadFileToAzureBlob();

	while (!terminationRequired);

	Log_Debug("App Exit\r\n");
	return 0;
}