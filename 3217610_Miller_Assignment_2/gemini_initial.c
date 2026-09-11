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
                 If multiple emails have the same sender category, the newest
                 email (by date MM-DD-YYYY) is prioritized first.
                 The program processes commands from an input file:
                   - EMAIL <sender>, <subject>, <date>: Adds email to queue
                   - NEXT: Displays the next email to read without removing it
                   - READ: Marks the highest priority email as read (removes it)
                   - COUNT: Displays count of unread emails
Inputs:          Input file containing commands (via command line argument,
                 prompt, or standard input).
Outputs:         Formatted terminal output for COUNT, NEXT, and empty states.
Collaborators:   None
Other Sources:   Google Gemini (GenAI assistance for EECS 348 Assignment 2)
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
#define MAX_SENDER_LEN   64
#define MAX_SUBJECT_LEN  256
#define MAX_DATE_LEN     16
#define INITIAL_CAPACITY 64
/*
 * Structure to represent a single email.
 */
typedef struct {
    char sender[MAX_SENDER_LEN];      // Sender category string
    char subject[MAX_SUBJECT_LEN];    // Subject line string
    char date[MAX_DATE_LEN];          // Date formatted as MM-DD-YYYY
    int priority;                     // Numeric priority: Boss=5 ... OtherPerson=1
    int date_val;                     // Normalized integer date YYYYMMDD for comparison
    int arrival_order;                // Monotonically increasing ID to preserve arrival order
} Email;
/*
 * Structure representing the list-based MaxHeap priority queue.
 */
typedef struct {
    Email *data;      // Dynamic array representing the binary heap tree
    int size;         // Number of elements currently in the heap
    int capacity;     // Total allocated capacity of the array
} MaxHeap;
/* Function Prototypes */
MaxHeap* create_heap(int initial_capacity);
void free_heap(MaxHeap *heap);
void swap_emails(Email *a, Email *b);
int compare_emails(const Email *a, const Email *b);
void heapify_up(MaxHeap *heap, int index);
void heapify_down(MaxHeap *heap, int index);
void insert_email(MaxHeap *heap, Email email);
Email* peek_max(MaxHeap *heap);
int extract_max(MaxHeap *heap, Email *out_email);
int get_sender_priority(const char *sender);
int parse_date_value(const char *date_str);
char* trim_whitespace(char *str);
void process_input(FILE *fp);
/*
 * Function: create_heap
 * Allocates and initializes a new list-based MaxHeap with a given initial capacity.
 */
MaxHeap* create_heap(int initial_capacity) {
    MaxHeap *heap = (MaxHeap *)malloc(sizeof(MaxHeap));
    if (!heap) {
        fprintf(stderr, "Error: Memory allocation failed for MaxHeap structure.\n");
        exit(EXIT_FAILURE);
    }
    heap->capacity = (initial_capacity > 0) ? initial_capacity : INITIAL_CAPACITY;
    heap->size = 0;
    heap->data = (Email *)malloc(sizeof(Email) * heap->capacity);
    if (!heap->data) {
        fprintf(stderr, "Error: Memory allocation failed for heap data array.\n");
        free(heap);
        exit(EXIT_FAILURE);
    }
    return heap;
}
/*
 * Function: free_heap
 * Frees all allocated memory associated with the MaxHeap.
 */
void free_heap(MaxHeap *heap) {
    if (heap) {
        if (heap->data) {
            free(heap->data);
        }
        free(heap);
    }
}
/*
 * Function: swap_emails
 * Swaps two Email objects in memory.
 */
void swap_emails(Email *a, Email *b) {
    Email temp = *a;
    *a = *b;
    *b = temp;
}
/*
 * Function: compare_emails
 * Compares two emails based on the priority rules:
 * 1. Sender category (Boss > Subordinate > Peer > ImportantPerson > OtherPerson)
 * 2. If same sender category: newest email (larger date value) comes first
 * 3. If both category and date match: earlier arrival order takes precedence (FIFO)
 * Returns:
 *   > 0 if a has higher priority than b
 *   < 0 if a has lower priority than b
 *     0 if equal
 */
int compare_emails(const Email *a, const Email *b) {
    // 1. Compare sender category priority
    if (a->priority != b->priority) {
        return a->priority - b->priority;
    }
    // 2. Sender category is identical: newest email (larger date value) comes first
    if (a->date_val != b->date_val) {
        return a->date_val - b->date_val;
    }
    // 3. Tie-breaker for identical date: preserve arrival order (earlier arrival first)
    return b->arrival_order - a->arrival_order;
}
/*
 * Function: heapify_up
 * Restores the MaxHeap property by moving the element at 'index' up the tree.
 * Parent index formula for 0-indexed list-based heap: (index - 1) / 2
 */
void heapify_up(MaxHeap *heap, int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        // If current child has higher priority than parent, swap them
        if (compare_emails(&heap->data[index], &heap->data[parent]) > 0) {
            swap_emails(&heap->data[index], &heap->data[parent]);
            index = parent;
        } else {
            break;
        }
    }
}
/*
 * Function: heapify_down
 * Restores the MaxHeap property by moving the element at 'index' down the tree.
 * Left child: 2 * index + 1, Right child: 2 * index + 2
 */
void heapify_down(MaxHeap *heap, int index) {
    while (1) {
        int left = 2 * index + 1;
        int right = 2 * index + 2;
        int largest = index;
        // Check if left child has higher priority than current largest
        if (left < heap->size && compare_emails(&heap->data[left], &heap->data[largest]) > 0) {
            largest = left;
        }
        // Check if right child has higher priority than current largest
        if (right < heap->size && compare_emails(&heap->data[right], &heap->data[largest]) > 0) {
            largest = right;
        }
        // If largest is not the current index, swap and continue sinking
        if (largest != index) {
            swap_emails(&heap->data[index], &heap->data[largest]);
            index = largest;
        } else {
            break;
        }
    }
}
/*
 * Function: insert_email
 * Inserts a new email into the MaxHeap and maintains the heap property.
 */
void insert_email(MaxHeap *heap, Email email) {
    // Dynamically expand capacity if the array is full
    if (heap->size >= heap->capacity) {
        heap->capacity *= 2;
        Email *new_data = (Email *)realloc(heap->data, sizeof(Email) * heap->capacity);
        if (!new_data) {
            fprintf(stderr, "Error: Memory reallocation failed during heap expansion.\n");
            exit(EXIT_FAILURE);
        }
        heap->data = new_data;
    }
    // Place new email at the end of the heap and bubble up
    int current_index = heap->size;
    heap->data[current_index] = email;
    heap->size++;
    heapify_up(heap, current_index);
}
/*
 * Function: peek_max
 * Returns a pointer to the highest priority email without removing it.
 * Returns NULL if the heap is empty.
 */
Email* peek_max(MaxHeap *heap) {
    if (heap->size == 0) {
        return NULL;
    }
    return &heap->data[0];
}
/*
 * Function: extract_max
 * Removes and returns the highest priority email from the MaxHeap.
 * Returns 1 on success, or 0 if heap is empty.
 */
int extract_max(MaxHeap *heap, Email *out_email) {
    if (heap->size == 0) {
        return 0; // Empty heap
    }
    if (out_email) {
        *out_email = heap->data[0];
    }
    // Replace root with the last element and heapify down
    heap->data[0] = heap->data[heap->size - 1];
    heap->size--;
    if (heap->size > 0) {
        heapify_down(heap, 0);
    }
    return 1;
}
/*
 * Function: get_sender_priority
 * Maps sender category string to integer priority rank:
 * Boss (5) > Subordinate (4) > Peer (3) > ImportantPerson (2) > OtherPerson (1)
 */
int get_sender_priority(const char *sender) {
    if (strcmp(sender, "Boss") == 0) return 5;
    if (strcmp(sender, "Subordinate") == 0) return 4;
    if (strcmp(sender, "Peer") == 0) return 3;
    if (strcmp(sender, "ImportantPerson") == 0) return 2;
    if (strcmp(sender, "OtherPerson") == 0) return 1;
    return 0; // Unknown category fallback
}
/*
 * Function: parse_date_value
 * Parses a date string in MM-DD-YYYY format and converts it into an integer
 * in the form YYYYMMDD so newer dates produce larger integer values.
 */
int parse_date_value(const char *date_str) {
    int month = 0, day = 0, year = 0;
    if (sscanf(date_str, "%d-%d-%d", &month, &day, &year) == 3) {
        return (year * 10000) + (month * 100) + day;
    }
    return 0;
}
/*
 * Function: trim_whitespace
 * Trims leading and trailing whitespace, carriage returns, and newlines from a string in place.
 */
char* trim_whitespace(char *str) {
    if (!str) return NULL;
    // Trim leading whitespace
    while (isspace((unsigned char)*str)) {
        str++;
    }
    if (*str == '\0') {
        return str;
    }
    // Trim trailing whitespace
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return str;
}
/*
 * Function: process_input
 * Reads commands from the given file stream line by line and executes them.
 */
void process_input(FILE *fp) {
    MaxHeap *heap = create_heap(INITIAL_CAPACITY);
    char line[1024];
    int global_order = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        // Strip trailing newline characters
        line[strcspn(line, "\r\n")] = '\0';
        char *trimmed_line = trim_whitespace(line);
        if (strlen(trimmed_line) == 0) {
            continue; // Skip empty lines
        }
        // 1. Process EMAIL command: EMAIL <sender category>, <subject line>, <date>
        if (strncmp(trimmed_line, "EMAIL", 5) == 0 && (trimmed_line[5] == ' ' || trimmed_line[5] == '\t')) {
            char *content = trimmed_line + 5;
            while (*content == ' ' || *content == '\t') content++;
            // Split on comma delimiters
            char *comma1 = strchr(content, ',');
            if (!comma1) continue;
            *comma1 = '\0';
            char *sender_part = trim_whitespace(content);
            char *after_comma1 = comma1 + 1;
            char *comma2 = strchr(after_comma1, ',');
            if (!comma2) continue;
            *comma2 = '\0';
            char *subject_part = trim_whitespace(after_comma1);
            char *date_part = trim_whitespace(comma2 + 1);
            Email new_email;
            memset(&new_email, 0, sizeof(Email));
            strncpy(new_email.sender, sender_part, sizeof(new_email.sender) - 1);
            strncpy(new_email.subject, subject_part, sizeof(new_email.subject) - 1);
            strncpy(new_email.date, date_part, sizeof(new_email.date) - 1);
            new_email.priority = get_sender_priority(new_email.sender);
            new_email.date_val = parse_date_value(new_email.date);
            new_email.arrival_order = ++global_order;
            insert_email(heap, new_email);
        }
        // 2. Process COUNT command
        else if (strcmp(trimmed_line, "COUNT") == 0) {
            printf("There are %d emails to read.\n", heap->size);
        }
        // 3. Process NEXT command
        else if (strcmp(trimmed_line, "NEXT") == 0) {
            Email *next = peek_max(heap);
            if (next != NULL) {
                printf("Next email:\n");
                printf("    Sender: %s\n", next->sender);
                printf("    Subject: %s\n", next->subject);
                printf("    Date: %s\n", next->date);
            } else {
                // If queue is empty when NEXT is called
                printf("No emails to read.\n");
            }
        }
        // 4. Process READ command
        else if (strcmp(trimmed_line, "READ") == 0) {
            // Delete highest priority email from queue without displaying it
            extract_max(heap, NULL);
        }
    }
    free_heap(heap);
}
/*
 * Function: main
 * Entry point for the program. Supports:
 *   1. Filename passed via command-line argument: ./main testfile.txt
 *   2. Input piped/redirected via standard input: ./main < testfile.txt
 *   3. Interactive execution prompting user for filename.
 */
int main(int argc, char *argv[]) {
    FILE *fp = NULL;
    if (argc > 1) {
        // Command-line argument provided
        fp = fopen(argv[1], "r");
        if (!fp) {
            fprintf(stderr, "Error: Could not open file '%s'\n", argv[1]);
            return EXIT_FAILURE;
        }
    } else {
        // No command-line argument: check if input is redirected from file/pipe
        if (!ISATTY(FILENO(stdin))) {
            fp = stdin;
        } else {
            // Interactive terminal: prompt user for input file path
            char filename[256];
            printf("Enter input file name: ");
            if (fgets(filename, sizeof(filename), stdin) != NULL) {
                filename[strcspn(filename, "\r\n")] = '\0';
                char *clean_name = trim_whitespace(filename);
                if (strlen(clean_name) > 0) {
                    fp = fopen(clean_name, "r");
                    if (!fp) {
                        fprintf(stderr, "Error: Could not open file '%s'\n", clean_name);
                        return EXIT_FAILURE;
                    }
                } else {
                    fp = stdin;
                }
            } else {
                return EXIT_SUCCESS;
            }
        }
    }
    // Process all email commands
    process_input(fp);
    // Close file if opened locally
    if (fp != stdin && fp != NULL) {
        fclose(fp);
    }
    return EXIT_SUCCESS;
}