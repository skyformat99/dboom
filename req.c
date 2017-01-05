/* req.c */
#include <curl/curl.h>
/* For now() */
#include <libdill.h>
#include "req.h"
#include "dboom.h"

/* libcurl write function. Drops all data. */
static size_t
dropDataCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    return size * nmemb;
}

void MakeRequest(const char* url, int timeout, struct reqstats *rsp)
{
    if(!rsp);

    /* TODO: is this the right way to use the curl easy interface?
       I.e once per request? I doubt it... */

    /* request timer */
    rsp->tm = 0;

    /* Must pass something to dropDataCallback */
    int dummy = 0;

    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        /* Follow redirection */ 
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        /* Send all data to this function  */ 
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dropDataCallback);
        /* Pass "dummy" pointer to callback function */ 
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&dummy);
        /* Be a friendly Web citizen and declare who is making the request */
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "dboom/1.0");
        /* Perform the request, res will get the return code */
        int64_t start = now();
        res = curl_easy_perform(curl);
        rsp->tm = now() - start;
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s",
                curl_easy_strerror(res));

        curl_easy_cleanup(curl);
    }
}
