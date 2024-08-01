#ifndef CSV_H
#define CSV_H

typedef char *csv_buf;

// Prepares buf for reading CSV. May modify the string behind s.
void csv_start(char *s, csv_buf *buf);

// Reads the next field from a line of CSV.
// At the end of the line, csv_read will return NULL.
char *csv_read(csv_buf *buf);

#endif
