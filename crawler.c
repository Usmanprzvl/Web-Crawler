#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>
#include <sqlite3.h>

#define NUM_THREADS 3
#define BUFFER_SIZE 4096

typedef struct {
    const char *url;
    const char *filename;
} ThreadData;

pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

void *fetch_content(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    CURL *curl;
    FILE *fp;

    curl = curl_easy_init();
    if (!curl) {
        printf("CURL initialization failed\n");
        return NULL;
    }

    fp = fopen(data->filename, "wb");
    if (!fp) {
        printf("Failed to open file: %s\n", data->filename);
        curl_easy_cleanup(curl);
        return NULL;
    }

    curl_easy_setopt(curl, CURLOPT_URL, data->url);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        printf("CURL Error: %s\n", curl_easy_strerror(res));
        fclose(fp);
        curl_easy_cleanup(curl);
        return NULL;
    }

    fclose(fp);
    curl_easy_cleanup(curl);

    // Lock database before modifying it
    pthread_mutex_lock(&db_mutex);

    sqlite3 *db;
    char *err_msg = NULL;
    int rc = sqlite3_open("web_content.db", &db);
    if (rc != SQLITE_OK) {
        printf("Cannot open database: %s\n", sqlite3_errmsg(db));
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }

    const char *create_table_sql = 
        "CREATE TABLE IF NOT EXISTS Content ("
        "filename TEXT, "
        "url TEXT, "
        "content TEXT);";
    sqlite3_exec(db, create_table_sql, 0, 0, &err_msg);

    const char *insert_sql = "INSERT INTO Content (filename, url, content) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, data->filename, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, data->url, -1, SQLITE_STATIC);

    FILE *file = fopen(data->filename, "r");
    if (!file) {
        printf("Failed to open file for reading: %s\n", data->filename);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *content = (char *)malloc(size + 1);
    if (!content) {
        printf("Memory allocation failed!\n");
        fclose(file);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }

    fread(content, size, 1, file);
    content[size] = '\0';
    sqlite3_bind_text(stmt, 3, content, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        printf("Failed to insert data: %s\n", sqlite3_errmsg(db));
    }

    free(content);
    fclose(file);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    pthread_mutex_unlock(&db_mutex);

    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    ThreadData thread_data[NUM_THREADS];

    const char *urls[NUM_THREADS] = {
        "https://www.ddu.edu.et/about",
        "https://www.ddu.edu.et/contact",
        "https://www.w3schools.com"
    };

    const char *filenames[NUM_THREADS] = {
        "ddu_about.html",
        "ddu_contact.html",
        "w3schools.html"
    };

    // Initialize SQLite database
    sqlite3 *db;
    sqlite3_open("web_content.db", &db);
    char *err_msg = NULL;

    const char *create_table_sql = 
        "CREATE TABLE IF NOT EXISTS Content ("
        "filename TEXT, "
        "url TEXT, "
        "content TEXT);";
    sqlite3_exec(db, create_table_sql, 0, 0, &err_msg);
    sqlite3_close(db);

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].url = urls[i];
        thread_data[i].filename = filenames[i];
        pthread_create(&threads[i], NULL, fetch_content, (void *)&thread_data[i]);
    }

    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Content fetched, saved, and indexed successfully.\n");

    return 0;
}

