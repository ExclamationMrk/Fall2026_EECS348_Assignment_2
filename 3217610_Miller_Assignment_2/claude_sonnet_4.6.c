/*
================================================================================
Name of Program: EECS 348 Assignment 2 - CEO Email Priority Queue
Description:     This program prioritizes incoming emails for a busy company CEO
                 using a list-based MaxHeap implementation of a Priority Queue.
                 Emails are prioritized based on the sender category:
                   1. Boss (Highest priority)
                   2. Subordinate
                   3. Peer
                   4. ImportantPerson
                   5. OtherPerson (Lowest priority)
                 If multiple emails share the same sender category, the newest
                 email (by date MM-DD-YYYY) is given higher priority.
                 If the date is also the same, the earlier-arriving email is
                 given priority (FIFO tie-breaker).
                 The program processes commands from an input file:
                   - EMAIL <sender>, <subject>, <date>  Adds email to queue
                   - NEXT   Displays the highest-priority email (no removal)
                   - READ   Removes the highest-priority email from the queue
                   - COUNT  Prints the number of unread emails
Inputs:          Input file path (command-line arg, stdin redirect, or prompt).
Outputs:         Formatted terminal output for COUNT, NEXT, and empty states.
Collaborators:   None
Other Sources:   None
Author:          Dylan Miller
Creation Date:   September 11, 2026
Revision Date:   September 11, 2026
Revisions:       Initial implementation from scratch.
================================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

/* Constants */
#define MAX_SENDER_LEN   64
#define MAX_SUBJECT_LEN  256
#define MAX_DATE_LEN     16
#define INITIAL_CAPACITY 64

/*
 * Email: one entry in the priority queue.
 *   priority      - integer rank (Boss=5 ... OtherPerson=1)
 *   date_val      - YYYYMMDD integer so larger == newer
 *   arrival_order - monotonically increasing insertion counter for FIFO tiebreak
 */
typedef struct {
    char sender[MAX_SENDER_LEN];
    char subject[MAX_SUBJECT_LEN];
    char date[MAX_DATE_LEN];
    int  priority;
    int  date_val;
    int  arrival_order;
} Email;

/*
 * MaxHeap: 0-indexed array-based binary max-heap.
 *   Parent of node i  : (i - 1) / 2
 *   Left  child of i  : 2*i + 1
 *   Right child of i  : 2*i + 2
 */
typedef struct {
    Email *data;
    int    size;
    int    capacity;
} MaxHeap;

/* Function Prototypes */
static int      sender_priority(const char *sender);
static int      date_to_int(const char *date);
static char    *trim(char *s);
static int      cmp(const Email *a, const Email *b);
static void     swap_emails(Email *a, Email *b);
static MaxHeap *heap_create(void);
static void     heap_free(MaxHeap *h);
static void     heap_push(MaxHeap *h, Email e);
static Email   *heap_peek(MaxHeap *h);
static int      heap_pop(MaxHeap *h, Email *out);
static void     sift_up(MaxHeap *h, int i);
static void     sift_down(MaxHeap *h, int i);
static void     run(FILE *fp);

/*
 * Function: sender_priority
 * Maps sender category string to integer priority rank:
 * Boss(5) > Subordinate(4) > Peer(3) > ImportantPerson(2) > OtherPerson(1)
 */
static int sender_priority(const char *sender) {
    if (strcmp(sender, "Boss")            == 0) return 5;
    if (strcmp(sender, "Subordinate")     == 0) return 4;
    if (strcmp(sender, "Peer")            == 0) return 3;
    if (strcmp(sender, "ImportantPerson") == 0) return 2;
    if (strcmp(sender, "OtherPerson")     == 0) return 1;
    return 0; /* unknown */
}

/*
 * Function: date_to_int
 * Parses "MM-DD-YYYY" and returns an integer YYYYMMDD so newer dates
 * produce larger values.
 */
static int date_to_int(const char *date) {
    int m = 0, d = 0, y = 0;
    if (sscanf(date, "%d-%d-%d", &m, &d, &y) == 3)
        return y * 10000 + m * 100 + d;
    return 0;
}

/*
 * Function: trim
 * Trims leading and trailing whitespace from a string in place and
 * returns a pointer to the first non-whitespace character.
 */
static char *trim(char *s) {
    if (!s) return NULL;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        *end-- = '\0';
    return s;
}

/*
 * Function: cmp
 * Compares two emails for max-heap ordering.
 * Returns > 0 if a has higher priority than b.
 *   Rule 1: higher sender priority wins.
 *   Rule 2: same category -> newer date wins.
 *   Rule 3: same date -> earlier arrival order wins (FIFO).
 */
static int cmp(const Email *a, const Email *b) {
    if (a->priority != b->priority)
        return a->priority - b->priority;
    if (a->date_val != b->date_val)
        return a->date_val - b->date_val;
    return b->arrival_order - a->arrival_order;
}

/*
 * Function: swap_emails
 * Swaps two Email structs in memory.
 */
static void swap_emails(Email *a, Email *b) {
    Email tmp = *a;
    *a = *b;
    *b = tmp;
}

/*
 * Function: heap_create
 * Allocates and initializes a new MaxHeap with INITIAL_CAPACITY.
 */
static MaxHeap *heap_create(void) {
    MaxHeap *h = malloc(sizeof(MaxHeap));
    if (!h) { fputs("Error: out of memory\n", stderr); exit(1); }
    h->capacity = INITIAL_CAPACITY;
    h->size     = 0;
    h->data     = malloc(sizeof(Email) * h->capacity);
    if (!h->data) { fputs("Error: out of memory\n", stderr); free(h); exit(1); }
    return h;
}

/*
 * Function: heap_free
 * Frees all memory associated with a MaxHeap.
 */
static void heap_free(MaxHeap *h) {
    if (h) { free(h->data); free(h); }
}

/*
 * Function: sift_up
 * Restores the heap property by bubbling element at index i upward.
 */
static void sift_up(MaxHeap *h, int i) {
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (cmp(&h->data[i], &h->data[parent]) > 0) {
            swap_emails(&h->data[i], &h->data[parent]);
            i = parent;
        } else {
            break;
        }
    }
}

/*
 * Function: sift_down
 * Restores the heap property by sinking element at index i downward.
 */
static void sift_down(MaxHeap *h, int i) {
    for (;;) {
        int left    = 2 * i + 1;
        int right   = 2 * i + 2;
        int largest = i;

        if (left  < h->size && cmp(&h->data[left],  &h->data[largest]) > 0) largest = left;
        if (right < h->size && cmp(&h->data[right], &h->data[largest]) > 0) largest = right;

        if (largest != i) {
            swap_emails(&h->data[i], &h->data[largest]);
            i = largest;
        } else {
            break;
        }
    }
}

/*
 * Function: heap_push
 * Inserts a new Email into the MaxHeap and maintains the heap property.
 * Doubles capacity when the array is full.
 */
static void heap_push(MaxHeap *h, Email e) {
    if (h->size >= h->capacity) {
        h->capacity *= 2;
        Email *tmp = realloc(h->data, sizeof(Email) * h->capacity);
        if (!tmp) { fputs("Error: out of memory\n", stderr); exit(1); }
        h->data = tmp;
    }
    h->data[h->size++] = e;
    sift_up(h, h->size - 1);
}

/*
 * Function: heap_peek
 * Returns a pointer to the highest-priority Email without removing it.
 * Returns NULL if the heap is empty.
 */
static Email *heap_peek(MaxHeap *h) {
    return (h->size > 0) ? &h->data[0] : NULL;
}

/*
 * Function: heap_pop
 * Removes the highest-priority Email from the heap.
 * Copies the removed email into *out if out is non-NULL.
 * Returns 1 on success, 0 if the heap was empty.
 */
static int heap_pop(MaxHeap *h, Email *out) {
    if (h->size == 0) return 0;
    if (out) *out = h->data[0];
    h->data[0] = h->data[--h->size];
    if (h->size > 0) sift_down(h, 0);
    return 1;
}

/*
 * Function: run
 * Reads commands line-by-line from fp and executes them against the heap.
 * Supported commands:
 *   EMAIL <sender>, <subject>, <date>
 *   COUNT
 *   NEXT
 *   READ
 */
static void run(FILE *fp) {
    MaxHeap *heap  = heap_create();
    char     line[1024];
    int      order = 0;

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *cmd = trim(line);
        if (!cmd || *cmd == '\0') continue;

        /* EMAIL */
        if (strncmp(cmd, "EMAIL", 5) == 0 && (cmd[5] == ' ' || cmd[5] == '\t')) {
            char *body = cmd + 5;
            while (*body == ' ' || *body == '\t') body++;

            char *c1 = strchr(body, ',');
            if (!c1) continue;
            *c1 = '\0';
            char *sender_s = trim(body);

            char *c2 = strchr(c1 + 1, ',');
            if (!c2) continue;
            *c2 = '\0';
            char *subject_s = trim(c1 + 1);
            char *date_s    = trim(c2 + 1);

            Email e;
            memset(&e, 0, sizeof(e));
            strncpy(e.sender,  sender_s,  sizeof(e.sender)  - 1);
            strncpy(e.subject, subject_s, sizeof(e.subject) - 1);
            strncpy(e.date,    date_s,    sizeof(e.date)    - 1);
            e.priority      = sender_priority(e.sender);
            e.date_val      = date_to_int(e.date);
            e.arrival_order = ++order;

            heap_push(heap, e);
        }
        /* COUNT */
        else if (strcmp(cmd, "COUNT") == 0) {
            printf("There are %d emails to read.\n", heap->size);
            putchar('\n');
        }
        /* NEXT */
        else if (strcmp(cmd, "NEXT") == 0) {
            Email *top = heap_peek(heap);
            if (top) {
                printf("Next email:\n");
                printf("    Sender: %s\n",  top->sender);
                printf("    Subject: %s\n", top->subject);
                printf("    Date: %s\n",    top->date);
            } else {
                printf("No emails to read.\n");
            }
            putchar('\n');
        }
        /* READ */
        else if (strcmp(cmd, "READ") == 0) {
            heap_pop(heap, NULL);
        }
    }

    heap_free(heap);
}

/*
 * Function: main
 * Entry point. Supports three input modes:
 *   1. ./main sample_input.txt   (command-line argument)
 *   2. ./main < sample_input.txt (stdin redirect / pipe)
 *   3. ./main                    (interactive: prompts for filename)
 */
int main(int argc, char *argv[]) {
    FILE *fp = NULL;

    if (argc > 1) {
        fp = fopen(argv[1], "r");
        if (!fp) {
            fprintf(stderr, "Error: cannot open '%s'\n", argv[1]);
            return EXIT_FAILURE;
        }
    } else if (!ISATTY(FILENO(stdin))) {
        fp = stdin;
    } else {
        char name[256];
        printf("Enter input file name: ");
        if (!fgets(name, sizeof(name), stdin)) return EXIT_SUCCESS;
        name[strcspn(name, "\r\n")] = '\0';
        char *clean = trim(name);
        if (*clean) {
            fp = fopen(clean, "r");
            if (!fp) {
                fprintf(stderr, "Error: cannot open '%s'\n", clean);
                return EXIT_FAILURE;
            }
        } else {
            fp = stdin;
        }
    }

    run(fp);

    if (fp != stdin) fclose(fp);
    return EXIT_SUCCESS;
}
