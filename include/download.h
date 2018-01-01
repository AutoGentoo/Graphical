#ifndef __AUTOGENTOO_DOWNLOAD_H__
#define __AUTOGENTOO_DOWNLOAD_H__

#include <response.h>

typedef enum {
    NO_PROGRESS,
    SHOW_PROGRESS
} down_progress;

/**
 * Download a file from the internet
 * @param url the url to download from
 * @param dest the file to download to
 * @param p Show/hide the progress bar
 * @return The HTTP response status
 */
response_t download (char* url, char* dest, down_progress p);

#endif