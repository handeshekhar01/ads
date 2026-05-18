
//  * Data Structures used:
//  *   Array    → main student storage
//  *   AVL Tree → CGPA range queries  (cgpa > x, cgpa < x)
//  *   Trie     → name / branch search
//  *   Max Heap → top-k students by CGPA
//  *   File I/O → persistent CSV storage in students.txt
//  * Compile : gcc -o spms2 spms2.c -lm
//  * Run     : ./spms2


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

//  *  SECTION 1 — CONSTANTS

#define MAX_STUDENTS         1000
#define MAX_NAME               50
#define MAX_BRANCH             30
#define MAX_SEMS                8
#define TRIE_ALPHA            128   /* full ASCII range              */
#define MAX_HEAP_CAP         1000
#define MAX_SKILLS              5
#define MAX_SKILL_LEN          30
#define MAX_COMPANIES         100
#define MAX_CO_NAME            50
#define MAX_BRANCHES_PER_CO    10

#define STUDENT_FILE  "students.txt"
#define COMPANY_FILE  "companies.txt"
#define EXPORT_FILE   "export.csv"

//  *  SECTION 2 — DATA STRUCTURES (Student & Company)

typedef struct {
    int   id;
    char  name[MAX_NAME];
    char  branch[MAX_BRANCH];
    float sgpa[MAX_SEMS];
    int   total_semesters;
    float cgpa;
    int   backlogs;
    int   is_placed;
    char  company_placed_in[MAX_CO_NAME];
    float package_lpa;
    int   skill_count;
    char  skills[MAX_SKILLS][MAX_SKILL_LEN];
} Student;

typedef struct {
    char  name[MAX_CO_NAME];
    float min_cgpa;
    float package_lpa;
    int   allow_backlogs;
    int   branch_count;
    char  allowed_branches[MAX_BRANCHES_PER_CO][MAX_BRANCH];
} Company;

/* All data lives in these two global arrays during runtime */
Student students[MAX_STUDENTS];
int     student_count = 0;
Company companies[MAX_COMPANIES];
int     company_count = 0;


//  *  SECTION 3 — UTILITY FUNCTIONS
//  *  Small helpers that get used everywhere — string cleanup,
//  *  case-insensitive compare, safe input reads, etc.


/* Lowercases a string in place */
void str_to_lower(char *s) {
    for (int i = 0; s[i]; i++)
        s[i] = (char)tolower((unsigned char)s[i]);
}

/* Strips leading/trailing spaces and newlines */
void trim(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1]==' '||s[len-1]=='\n'||s[len-1]=='\r'))
        s[--len] = '\0';
    int start = 0;
    while (s[start]==' ') start++;
    if (start > 0) memmove(s, s+start, (size_t)(len-start+1));
}

/* Returns 1 if two strings match, ignoring case */
int str_eq_nocase(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return (*a == '\0' && *b == '\0');
}

/* Walks the array to find a student by ID */
Student *find_by_id(int id) {
    for (int i = 0; i < student_count; i++)
        if (students[i].id == id) return &students[i];
    return NULL;
}

int is_dup_id(int id) { return find_by_id(id) != NULL; }

/*
 * Three input helpers to avoid repeating scanf/fgets everywhere.
 *   read_int_opt   → returns fallback if user just presses Enter
 *   read_float_opt → same idea, for floats
 *   read_int_req   → required int, returns -1 if something goes wrong
 */
static int read_int_opt(int fallback) {
    char buf[32];
    if (!fgets(buf, sizeof(buf), stdin)) return fallback;
    trim(buf);
    return strlen(buf) ? atoi(buf) : fallback;
}

static float read_float_opt(float fallback) {
    char buf[32];
    if (!fgets(buf, sizeof(buf), stdin)) return fallback;
    trim(buf);
    return strlen(buf) ? (float)atof(buf) : fallback;
}

static int read_int_req(void) {
    int v;
    if (scanf("%d", &v) != 1) { while(getchar()!='\n'); return -1; }
    getchar();
    return v;
}


//  *  SECTION 4 — DISPLAY FUNCTIONS
//  *  Two table layouts: academic view and placement view.
//  *  sep/hdr/row helpers keep printing DRY and consistent.

/* Academic table: ID | Name | Branch | SGPAs | CGPA */
void sep_acad(void) {
    printf("------+----------------------+------------+----------------------------------+------\n");
}
void hdr_acad(void) {
    sep_acad();
    printf(" %-4s | %-20s | %-10s | %-32s | %s\n",
           "ID","Name","Branch","SGPA (semester-wise)","CGPA");
    sep_acad();
}
void row_acad(const Student *s) {
    char buf[90]="", tmp[14];
    for (int i=0; i<s->total_semesters; i++) {
        sprintf(tmp,"%.2f",s->sgpa[i]);
        if (i>0) strcat(buf,", ");
        strcat(buf,tmp);
    }
    printf(" %-4d | %-20s | %-10s | %-32s | %.2f\n",
           s->id,s->name,s->branch,buf,s->cgpa);
}

/* Placement table: ID | Name | Branch | CGPA | Status | Company | Pkg | Skills */
void sep_place(void) {
    printf("------+--------------------+--------+------+---------+-----------+---------+----------------------------\n");
}
void hdr_place(void) {
    sep_place();
    printf(" %-4s | %-18s | %-6s | %-4s | %-7s | %-9s | %-7s | %s\n",
           "ID","Name","Branch","CGPA","Status","Company","Pkg LPA","Skills");
    sep_place();
}
void row_place(const Student *s) {
    char sk[120]="";
    for (int i=0; i<s->skill_count; i++) {
        if (i>0) strcat(sk,", ");
        strcat(sk,s->skills[i]);
    }
    if (!s->skill_count) strcpy(sk,"-");
    printf(" %-4d | %-18s | %-6s | %-4.2f | %-7s | %-9s | %-7.2f | %s\n",
           s->id, s->name, s->branch, s->cgpa,
           s->is_placed ? "Placed" : "Seeking",
           s->is_placed ? s->company_placed_in : "-",
           s->is_placed ? s->package_lpa : 0.0f, sk);
}


//  *  SECTION 5 — AVL TREE  [Self-Balancing BST for CGPA Queries]
//  * ----------------------------------------------------------------
//  *  Why AVL and not a plain BST?
//  *    If students are added in sorted CGPA order, a plain BST turns
//  *    into a linked list — O(n) per search. AVL keeps itself balanced
//  *    using rotations, so search is always O(log n).
//  *
//  *  Balance factor = height(left) - height(right).
//  *  If it ever lands outside {-1, 0, 1}, we rotate to fix it:
//  *    Left-Left   → Right Rotate
//  *    Right-Right → Left Rotate
//  *    Left-Right  → Left Rotate on child, then Right Rotate on root
//  *    Right-Left  → Right Rotate on child, then Left Rotate on root
//  *
//  *  Each node's key is a CGPA float. Since students can share a
//  *  CGPA, each node also holds a small linked list of student IDs.
//  *  Used by: cgpa > x  and  cgpa < x  queries.


/* Linked list of IDs stored at each AVL node (same-CGPA students) */
typedef struct IDNode { int id; struct IDNode *next; } IDNode;

typedef struct AVLNode {
    float   key;            /* the CGPA value this node represents */
    IDNode *ids;            /* students sharing this exact CGPA     */
    int     height;
    struct AVLNode *left, *right;
} AVLNode;

AVLNode *avl_root = NULL;

static int avl_h(const AVLNode *n)  { return n ? n->height : 0; }
static int avl_bf(const AVLNode *n) { return n ? avl_h(n->left) - avl_h(n->right) : 0; }
static void avl_fix_height(AVLNode *n) {
    int lh = avl_h(n->left), rh = avl_h(n->right);
    n->height = 1 + (lh > rh ? lh : rh);
}

static AVLNode *avl_rotate_right(AVLNode *y) {
    AVLNode *x = y->left, *B = x->right;
    x->right = y; y->left = B;
    avl_fix_height(y); avl_fix_height(x);
    return x;
}

static AVLNode *avl_rotate_left(AVLNode *x) {
    AVLNode *y = x->right, *B = y->left;
    y->left = x; x->right = B;
    avl_fix_height(x); avl_fix_height(y);
    return y;
}

/* Allocates a fresh AVL node seeded with one student ID */
static AVLNode *avl_new_node(float key, int id) {
    AVLNode *n = (AVLNode *)malloc(sizeof(AVLNode));
    IDNode  *e = (IDNode  *)malloc(sizeof(IDNode));
    e->id = id; e->next = NULL;
    n->key = key; n->ids = e; n->height = 1;
    n->left = n->right = NULL;
    return n;
}

/*
 * Standard BST insert, then re-balance on the way back up.
 * Duplicate CGPA? Just append the ID to that node's list.
 */
AVLNode *avl_insert(AVLNode *node, float key, int id) {
    if (!node) return avl_new_node(key, id);
    if      (key < node->key) node->left  = avl_insert(node->left,  key, id);
    else if (key > node->key) node->right = avl_insert(node->right, key, id);
    else {
        IDNode *e = (IDNode *)malloc(sizeof(IDNode));
        e->id = id; e->next = node->ids; node->ids = e;
        return node;
    }
    avl_fix_height(node);
    int bf = avl_bf(node);
    if (bf >  1 && key < node->left->key)  return avl_rotate_right(node);
    if (bf < -1 && key > node->right->key) return avl_rotate_left(node);
    if (bf >  1 && key > node->left->key)  {
        node->left = avl_rotate_left(node->left);
        return avl_rotate_right(node);
    }
    if (bf < -1 && key < node->right->key) {
        node->right = avl_rotate_right(node->right);
        return avl_rotate_left(node);
    }
    return node;
}

/* Prints every student stored at one node */
static void avl_print_node(const AVLNode *n) {
    const IDNode *c = n->ids;
    while (c) { Student *s = find_by_id(c->id); if (s) row_acad(s); c = c->next; }
}

/* Inorder walk — only visits nodes where CGPA > threshold */
void avl_gt(const AVLNode *n, float t) {
    if (!n) return;
    if (n->key > t) { avl_gt(n->left, t); avl_print_node(n); avl_gt(n->right, t); }
    else              avl_gt(n->right, t);
}

/* Inorder walk — only visits nodes where CGPA < threshold */
void avl_lt(const AVLNode *n, float t) {
    if (!n) return;
    if (n->key < t) { avl_lt(n->left, t); avl_print_node(n); avl_lt(n->right, t); }
    else              avl_lt(n->left, t);
}

/* Recursively frees the whole tree */
static void avl_free(AVLNode *n) {
    if (!n) return;
    avl_free(n->left); avl_free(n->right);
    IDNode *id = n->ids;
    while (id) { IDNode *nx = id->next; free(id); id = nx; }
    free(n);
}

//  *  SECTION 6 — TRIE  [Prefix Tree for Name & Branch Search]
//  * ----------------------------------------------------------------
//  *  Why a Trie over a simple array scan?
//  *    Array scan for a name is O(n × m) — every student, every char.
//  *    Trie lookup is O(m) regardless of how many students exist.
//  *
//  *  Inserting "rahul": r → a → h → u → l → mark end, attach ID list.
//  *  Searching "rahul": follow the same path — if it ends at an
//  *  is_end node, we found our matches.
//  *
//  *  We keep two separate tries: one for names, one for branches.
//  *  Used by: name = x  and  branch = x  queries.

typedef struct TIDNode { int id; struct TIDNode *next; } TIDNode;

typedef struct TrieNode {
    struct TrieNode *ch[TRIE_ALPHA]; /* one slot per ASCII character */
    int     is_end;
    TIDNode *ids;
} TrieNode;

TrieNode *name_trie   = NULL;
TrieNode *branch_trie = NULL;

/* calloc gives us all-NULL children for free */
static TrieNode *trie_new(void) {
    return (TrieNode *)calloc(1, sizeof(TrieNode));
}

/* Walk character by character, creating nodes as needed */
void trie_insert(TrieNode *root, const char *word, int id) {
    TrieNode *cur = root;
    for (int i = 0; word[i]; i++) {
        int idx = (unsigned char)word[i];
        if (!cur->ch[idx]) cur->ch[idx] = trie_new();
        cur = cur->ch[idx];
    }
    cur->is_end = 1;
    TIDNode *e = (TIDNode *)malloc(sizeof(TIDNode));
    e->id = id; e->next = cur->ids; cur->ids = e;
}

/* Follow the path; return 0 if any character is missing */
int trie_search(TrieNode *root, const char *word) {
    TrieNode *cur = root;
    for (int i = 0; word[i]; i++) {
        int idx = (unsigned char)word[i];
        if (!cur->ch[idx]) return 0;
        cur = cur->ch[idx];
    }
    if (!cur->is_end) return 0;
    int count = 0;
    TIDNode *e = cur->ids;
    while (e) { Student *s = find_by_id(e->id); if (s) { row_acad(s); count++; } e = e->next; }
    return count;
}

static void trie_free(TrieNode *n) {
    if (!n) return;
    for (int i = 0; i < TRIE_ALPHA; i++) trie_free(n->ch[i]);
    TIDNode *id = n->ids;
    while (id) { TIDNode *nx = id->next; free(id); id = nx; }
    free(n);
}

//  *  SECTION 7 — MAX HEAP  [Top-K Students by CGPA]
//  * ----------------------------------------------------------------
//  *  Why a Heap instead of sorting?
//  *    Sorting the full array every time someone asks "top 5" costs
//  *    O(n log n). A heap keeps the max at index 0 always, so
//  *    pulling the top k costs only O(k log n).
//  *
//  *  Array-based layout (parent/child positions):
//  *    Node i  →  parent = (i-1)/2,  left = 2i+1,  right = 2i+2
//  *
//  *    INSERT : append at end, sift UP   (swap with parent while bigger)
//  *    EXTRACT: swap root with last, sift DOWN (swap with bigger child)
//  *
//  *  Used by: top k  query.

typedef struct { float cgpa; int id; } HEntry;

HEntry heap_arr[MAX_HEAP_CAP];
int    heap_sz = 0;

static void hswap(HEntry *a, HEntry *b) { HEntry t = *a; *a = *b; *b = t; }

/* After appending at end, bubble up until parent is larger */
static void sift_up(HEntry *h, int i) {
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h[parent].cgpa < h[i].cgpa) { hswap(&h[parent], &h[i]); i = parent; }
        else break;
    }
}

/* After removing root, push the replacement down until heap is valid */
static void sift_dn(HEntry *h, int sz, int i) {
    while (1) {
        int left = 2*i+1, right = 2*i+2, largest = i;
        if (left  < sz && h[left].cgpa  > h[largest].cgpa) largest = left;
        if (right < sz && h[right].cgpa > h[largest].cgpa) largest = right;
        if (largest != i) { hswap(&h[largest], &h[i]); i = largest; }
        else break;
    }
}

void heap_insert(float cgpa, int id) {
    if (heap_sz >= MAX_HEAP_CAP) return;
    heap_arr[heap_sz].cgpa = cgpa;
    heap_arr[heap_sz].id   = id;
    sift_up(heap_arr, heap_sz);
    heap_sz++;
}

/*
 * Works on a copy of the heap so the real one stays intact.
 * Each round: print root (current max), replace with last, sift down.
 */
void heap_top_k(int k) {
    if (heap_sz == 0) { printf("  No students.\n"); return; }
    if (k > heap_sz)  k = heap_sz;
    if (k <= 0)       { printf("  [Error] k must be positive.\n"); return; }

    HEntry tmp[MAX_HEAP_CAP];
    int    tsz = heap_sz;
    memcpy(tmp, heap_arr, (size_t)heap_sz * sizeof(HEntry));

    hdr_acad();
    for (int i = 0; i < k; i++) {
        Student *s = find_by_id(tmp[0].id);
        if (s) row_acad(s);
        tmp[0] = tmp[--tsz];
        sift_dn(tmp, tsz, 0);
    }
    sep_acad();
}

//  *  SECTION 8 — INDEX REBUILD
//  *  Called after every update or delete so AVL, Heap, and Tries
//  *  always match what's actually in the students[] array.
//  *  Without this, stale results would show up after edits.


void reg_student(Student *s); /* defined below, declared here to avoid reordering */

void rebuild_indexes(void) {
    avl_free(avl_root);       avl_root    = NULL;
    trie_free(name_trie);     name_trie   = trie_new();
    trie_free(branch_trie);   branch_trie = trie_new();
    heap_sz = 0;
    for (int i = 0; i < student_count; i++)
        reg_student(&students[i]);
}


//  *  SECTION 9 — FILE HANDLING  [CSV Persistence]
//  *  Reads and writes students.txt and companies.txt in CSV format.
//  *  save_all rewrites everything; append_student adds one line.

static void write_student_line(FILE *f, const Student *s) {
    fprintf(f, "%d,%s,%s,%d", s->id, s->name, s->branch, s->total_semesters);
    for (int i = 0; i < s->total_semesters; i++) fprintf(f, ",%.2f", s->sgpa[i]);
    fprintf(f, ",%.2f,%d,%s,%.2f,%d,%d",
            s->cgpa, s->is_placed,
            strlen(s->company_placed_in) ? s->company_placed_in : "none",
            s->package_lpa, s->backlogs, s->skill_count);
    for (int i = 0; i < s->skill_count; i++) fprintf(f, ",%s", s->skills[i]);
    fprintf(f, "\n");
}

/* Full rewrite — used after updates or deletes */
void save_all(void) {
    FILE *f = fopen(STUDENT_FILE, "w");
    if (!f) { perror("Cannot write students.txt"); return; }
    for (int i = 0; i < student_count; i++) write_student_line(f, &students[i]);
    fclose(f);
}

/* Faster path — just tack one line onto the end */
void append_student(const Student *s) {
    FILE *f = fopen(STUDENT_FILE, "a");
    if (!f) { perror("Cannot open students.txt"); return; }
    write_student_line(f, s); fclose(f);
}

/* Inserts a student into all three indexes at once */
void reg_student(Student *s) {
    avl_root = avl_insert(avl_root, s->cgpa, s->id);
    char ln[MAX_NAME], lb[MAX_BRANCH];
    strncpy(ln, s->name,   MAX_NAME-1);   ln[MAX_NAME-1]   = '\0';
    strncpy(lb, s->branch, MAX_BRANCH-1); lb[MAX_BRANCH-1] = '\0';
    str_to_lower(ln); str_to_lower(lb);
    trie_insert(name_trie,   ln, s->id);
    trie_insert(branch_trie, lb, s->id);
    heap_insert(s->cgpa, s->id);
}

/* Parses students.txt line by line into the students[] array */
void load_students(void) {
    FILE *f = fopen(STUDENT_FILE, "r"); if (!f) return;
    char line[1024];
    while (fgets(line, sizeof(line), f) && student_count < MAX_STUDENTS) {
        Student *s = &students[student_count];
        memset(s, 0, sizeof(Student));
        char *tok;
        tok = strtok(line, ","); if (!tok) continue; s->id = atoi(tok);
        tok = strtok(NULL, ","); if (!tok) continue; strncpy(s->name,   tok, MAX_NAME-1);
        tok = strtok(NULL, ","); if (!tok) continue; strncpy(s->branch, tok, MAX_BRANCH-1);
        tok = strtok(NULL, ","); if (!tok) continue; s->total_semesters = atoi(tok);
        for (int i = 0; i < s->total_semesters; i++) {
            tok = strtok(NULL, ","); if (!tok) break; s->sgpa[i] = (float)atof(tok);
        }
        tok = strtok(NULL, ","); if (!tok) { reg_student(s); student_count++; continue; }
        s->cgpa = (float)atof(tok);
        tok = strtok(NULL, ","); if (!tok) { reg_student(s); student_count++; continue; }
        s->is_placed = atoi(tok);
        tok = strtok(NULL, ","); if (!tok) { reg_student(s); student_count++; continue; }
        strncpy(s->company_placed_in, tok, MAX_CO_NAME-1); trim(s->company_placed_in);
        tok = strtok(NULL, ","); if (!tok) { reg_student(s); student_count++; continue; }
        s->package_lpa = (float)atof(tok);
        tok = strtok(NULL, ","); if (!tok) { reg_student(s); student_count++; continue; }
        s->backlogs = atoi(tok);
        tok = strtok(NULL, ","); if (!tok) { reg_student(s); student_count++; continue; }
        s->skill_count = atoi(tok);
        if (s->skill_count > MAX_SKILLS) s->skill_count = MAX_SKILLS;
        for (int i = 0; i < s->skill_count; i++) {
            tok = strtok(NULL, ",\n\r"); if (!tok) break;
            strncpy(s->skills[i], tok, MAX_SKILL_LEN-1); trim(s->skills[i]);
        }
        reg_student(s); student_count++;
    }
    fclose(f);
}

void save_companies(void) {
    FILE *f = fopen(COMPANY_FILE, "w"); if (!f) return;
    for (int i = 0; i < company_count; i++) {
        Company *c = &companies[i];
        fprintf(f, "%s,%.2f,%.2f,%d,%d", c->name, c->min_cgpa, c->package_lpa,
                c->allow_backlogs, c->branch_count);
        for (int j = 0; j < c->branch_count; j++) fprintf(f, ",%s", c->allowed_branches[j]);
        fprintf(f, "\n");
    }
    fclose(f);
}

void load_companies(void) {
    FILE *f = fopen(COMPANY_FILE, "r"); if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f) && company_count < MAX_COMPANIES) {
        Company *c = &companies[company_count]; memset(c, 0, sizeof(Company));
        char *tok;
        tok = strtok(line, ","); if (!tok) continue; strncpy(c->name, tok, MAX_CO_NAME-1);
        tok = strtok(NULL, ","); if (!tok) continue; c->min_cgpa      = (float)atof(tok);
        tok = strtok(NULL, ","); if (!tok) continue; c->package_lpa   = (float)atof(tok);
        tok = strtok(NULL, ","); if (!tok) continue; c->allow_backlogs = atoi(tok);
        tok = strtok(NULL, ","); if (!tok) continue; c->branch_count  = atoi(tok);
        for (int j = 0; j < c->branch_count; j++) {
            tok = strtok(NULL, ",\n\r"); if (!tok) break;
            strncpy(c->allowed_branches[j], tok, MAX_BRANCH-1); trim(c->allowed_branches[j]);
        }
        company_count++;
    }
    fclose(f);
}


//  *  SECTION 10 — ADD STUDENT
//  *  Walks the user through filling in every field with validation.
//  *  CGPA is computed automatically from the SGPAs they enter.


void add_student(void) {
    if (student_count >= MAX_STUDENTS) { printf("  [Error] Student limit reached.\n"); return; }
    Student s; memset(&s, 0, sizeof(Student));

    printf("  Student ID         : ");
    s.id = read_int_req();
    if (s.id < 0) return;
    if (is_dup_id(s.id)) { printf("  [Error] ID %d already exists.\n", s.id); return; }

    printf("  Full Name          : ");
    if (!fgets(s.name, MAX_NAME, stdin)) return;
    trim(s.name);
    if (!strlen(s.name)) { printf("  [Error] Name empty.\n"); return; }

    const char *valid_branches[] = {"CS","IT","AIML","AIDS","AI"};
    while (1) {
        printf("  Branch (CS/IT/AIML/AIDS/AI): ");
        if (!fgets(s.branch, MAX_BRANCH, stdin)) return;
        trim(s.branch);
        for (int k = 0; s.branch[k]; k++)
            s.branch[k] = (char)toupper((unsigned char)s.branch[k]);
        int valid = 0;
        for (int k = 0; k < 5; k++)
            if (strcmp(s.branch, valid_branches[k]) == 0) { valid = 1; break; }
        if (valid) break;
        printf("  [Error] Only allowed: CS | IT | AIML | AIDS | AI\n");
    }

    printf("  Semesters (1-8)    : ");
    s.total_semesters = read_int_req();
    if (s.total_semesters < 1 || s.total_semesters > MAX_SEMS) {
        printf("  [Error] Must be 1-8.\n"); return;
    }

    float total = 0.0f;
    for (int i = 0; i < s.total_semesters; i++) {
        float v;
        while (1) {
            printf("  SGPA Sem %-2d        : ", i+1);
            if (scanf("%f", &v) != 1) { while(getchar()!='\n'); continue; }
            if (v < 0.0f || v > 10.0f) { printf("  [Error] Must be 0.0-10.0.\n"); continue; }
            break;
        }
        s.sgpa[i] = v; total += v;
    }
    getchar();
    s.cgpa = roundf((total / (float)s.total_semesters) * 100.0f) / 100.0f;

    printf("  Active Backlogs    : ");
    s.backlogs = read_int_opt(0);
    if (s.backlogs < 0) s.backlogs = 0;

    printf("  No. of Skills (0-%d): ", MAX_SKILLS);
    s.skill_count = read_int_opt(0);
    if (s.skill_count < 0) s.skill_count = 0;
    if (s.skill_count > MAX_SKILLS) s.skill_count = MAX_SKILLS;

    for (int i = 0; i < s.skill_count; i++) {
        printf("  Skill %-2d           : ", i+1);
        if (!fgets(s.skills[i], MAX_SKILL_LEN, stdin)) break;
        trim(s.skills[i]); str_to_lower(s.skills[i]);
    }

    s.is_placed = 0; strcpy(s.company_placed_in, "none"); s.package_lpa = 0.0f;
    students[student_count++] = s;
    reg_student(&students[student_count-1]);
    append_student(&students[student_count-1]);
    printf("\n  Student added! Computed CGPA = %.2f\n", s.cgpa);
}

//  *  SECTION 11 — UPDATE STUDENT
//  *  Shows current values in [brackets] — Enter keeps them.
//  *  Only asks for brand-new semesters if the count goes up.
//  *  Saves and rebuilds all indexes when done.

void update_student(int prn) {
    Student *s = find_by_id(prn);
    if (!s) { printf("  [Error] PRN %d not found.\n", prn); return; }

    printf("\n  Current Record:\n");
    printf("  %-18s: %s\n",  "Name",      s->name);
    printf("  %-18s: %s\n",  "Branch",    s->branch);
    printf("  %-18s: %d\n",  "Semesters", s->total_semesters);
    for (int i = 0; i < s->total_semesters; i++)
        printf("  SGPA Sem %-2d (cur)  : %.2f\n", i+1, s->sgpa[i]);
    printf("  %-18s: %.2f\n", "CGPA",     s->cgpa);
    printf("  %-18s: %d\n",  "Backlogs",  s->backlogs);
    printf("  %-18s: ",       "Skills");
    for (int i = 0; i < s->skill_count; i++)
        printf("%s%s", s->skills[i], i < s->skill_count-1 ? ", " : "");
    if (!s->skill_count) printf("none");
    printf("\n\n  (Press Enter on any field to keep the current value)\n\n");

    printf("  Semesters (1-8) [%d]: ", s->total_semesters);
    int new_sems = read_int_opt(s->total_semesters);
    if (new_sems < 1 || new_sems > MAX_SEMS) { printf("  [Error] Must be 1-8.\n"); return; }

    float new_sgpa[MAX_SEMS], total = 0.0f;
    for (int i = 0; i < new_sems; i++) {
        if (i < s->total_semesters) {
            printf("  SGPA Sem %-2d [%.2f]  : ", i+1, s->sgpa[i]);
            float v = read_float_opt(s->sgpa[i]);
            if (v < 0.0f || v > 10.0f) { printf("  [Error] Must be 0.0-10.0.\n"); return; }
            new_sgpa[i] = v;
        } else {
            printf("  SGPA Sem %-2d (new)   : ", i+1);
            float v = read_float_opt(-1.0f);
            if (v < 0.0f || v > 10.0f) {
                printf("  [Error] SGPA required for sem %d (0.0-10.0).\n", i+1); return;
            }
            new_sgpa[i] = v;
        }
        total += new_sgpa[i];
    }

    printf("  Backlogs       [%d]: ", s->backlogs);
    int new_bl = read_int_opt(s->backlogs);
    if (new_bl < 0) new_bl = 0;

    printf("  Skills (0-%d)   [%d]: ", MAX_SKILLS, s->skill_count);
    int new_sc = read_int_opt(s->skill_count);
    if (new_sc < 0) new_sc = 0;
    if (new_sc > MAX_SKILLS) new_sc = MAX_SKILLS;

    char new_skills[MAX_SKILLS][MAX_SKILL_LEN];
    for (int i = 0; i < new_sc; i++) {
        if (i < s->skill_count)
            printf("  Skill %-2d [%s]: ", i+1, s->skills[i]);
        else
            printf("  Skill %-2d (new)     : ", i+1);
        if (!fgets(new_skills[i], MAX_SKILL_LEN, stdin)) break;
        trim(new_skills[i]);
        if (!strlen(new_skills[i]) && i < s->skill_count)
            strncpy(new_skills[i], s->skills[i], MAX_SKILL_LEN-1);
        str_to_lower(new_skills[i]);
    }

    /* Commit everything */
    s->total_semesters = new_sems;
    for (int i = 0; i < new_sems; i++) s->sgpa[i] = new_sgpa[i];
    s->cgpa        = roundf((total / (float)new_sems) * 100.0f) / 100.0f;
    s->backlogs    = new_bl;
    s->skill_count = new_sc;
    for (int i = 0; i < new_sc; i++)
        strncpy(s->skills[i], new_skills[i], MAX_SKILL_LEN-1);

    save_all();
    rebuild_indexes();
    printf("\n  PRN %d (%s) updated. New CGPA = %.2f\n", prn, s->name, s->cgpa);
}


//  *  SECTION 12 — DELETE STUDENT
//  *  Shifts the array left to fill the gap, then saves and rebuilds.
//  *  Simple but effective for up to MAX_STUDENTS records.

void delete_student(int prn) {
    int idx = -1;
    for (int i = 0; i < student_count; i++)
        if (students[i].id == prn) { idx = i; break; }
    if (idx < 0) { printf("  [Error] PRN %d not found.\n", prn); return; }

    char name_copy[MAX_NAME];
    strncpy(name_copy, students[idx].name, MAX_NAME-1);

    for (int i = idx; i < student_count-1; i++)
        students[i] = students[i+1];
    student_count--;

    save_all();
    rebuild_indexes();
    printf("  PRN %d (%s) deleted successfully.\n", prn, name_copy);
}

//  *  SECTION 13 — PLACEMENT STATUS
//  *  One function handles both placed and unplaced views by flag.
//  *  mark_placement parses "mark <id> placed <Co> <pkg>" or unplaced.

void show_by_placement(int placed_flag) {
    int c = 0;
    hdr_place();
    for (int i = 0; i < student_count; i++)
        if (students[i].is_placed == placed_flag) { row_place(&students[i]); c++; }
    sep_place();
    if (placed_flag)
        printf(c ? "  Total placed: %d\n" : "  No placed students yet.\n", c);
    else
        printf(c ? "  Total unplaced: %d\n" : "  All students are placed!\n", c);
}

void mark_placement(char *args) {
    char buf[256]; strncpy(buf, args, sizeof(buf)-1); buf[sizeof(buf)-1]='\0'; trim(buf);
    char *tok = strtok(buf, " ");
    if (!tok) { printf("  [Error] Usage: mark <id> placed <company> <pkg>\n"); return; }
    int id = atoi(tok);
    Student *s = find_by_id(id);
    if (!s) { printf("  [Error] Student ID %d not found.\n", id); return; }

    tok = strtok(NULL, " ");
    if (!tok) { printf("  [Error] Specify 'placed' or 'unplaced'.\n"); return; }

    if (strcmp(tok, "unplaced") == 0) {
        s->is_placed = 0; strcpy(s->company_placed_in, "none"); s->package_lpa = 0.0f;
        save_all();
        printf("  Student %d (%s) → Unplaced.\n", id, s->name); return;
    }
    if (strcmp(tok, "placed") == 0) {
        tok = strtok(NULL, " ");
        if (!tok) { printf("  [Error] Specify company name.\n"); return; }
        strncpy(s->company_placed_in, tok, MAX_CO_NAME-1);
        tok = strtok(NULL, " ");
        s->package_lpa = tok ? (float)atof(tok) : 0.0f;
        s->is_placed = 1; save_all();
        printf("  Student %d (%s) → Placed at %s | %.2f LPA\n",
               id, s->name, s->company_placed_in, s->package_lpa); return;
    }
    printf("  [Error] Use 'placed' or 'unplaced'.\n");
}

//  *  SECTION 14 — COMPANY & ELIGIBILITY
//  *  add_company collects rules (min CGPA, backlog policy, branches).
//  *  show_eligible runs every student through is_eligible() and lists
//  *  whoever clears all three filters.

void add_company(void) {
    if (company_count >= MAX_COMPANIES) { printf("  [Error] Company limit reached.\n"); return; }
    Company c; memset(&c, 0, sizeof(Company));

    printf("  Company Name             : ");
    if (!fgets(c.name, MAX_CO_NAME, stdin)) return; trim(c.name);
    if (!strlen(c.name)) { printf("  [Error] Name empty.\n"); return; }

    printf("  Min CGPA Required        : ");
    c.min_cgpa = read_float_opt(0.0f);

    printf("  Package (LPA)            : ");
    c.package_lpa = read_float_opt(0.0f);

    printf("  Allow Backlogs (1=Yes/0=No): ");
    c.allow_backlogs = read_int_opt(0);

    printf("  No. of Branches (0=ALL)  : ");
    c.branch_count = read_int_opt(0);
    if (c.branch_count < 0) c.branch_count = 0;
    if (c.branch_count > MAX_BRANCHES_PER_CO) c.branch_count = MAX_BRANCHES_PER_CO;

    const char *vbr[] = {"cs","it","aiml","aids","ai"};
    for (int i = 0; i < c.branch_count; ) {
        printf("  Branch %-2d (CS/IT/AIML/AIDS/AI): ", i+1);
        if (!fgets(c.allowed_branches[i], MAX_BRANCH, stdin)) break;
        trim(c.allowed_branches[i]); str_to_lower(c.allowed_branches[i]);
        int ok = 0;
        for (int k = 0; k < 5; k++)
            if (strcmp(c.allowed_branches[i], vbr[k]) == 0) { ok = 1; break; }
        if (!ok) { printf("  [Error] Only: CS | IT | AIML | AIDS | AI\n"); continue; }
        i++;
    }
    companies[company_count++] = c; save_companies();
    printf("\n  Company '%s' added (Min CGPA: %.2f | %.2f LPA).\n",
           c.name, c.min_cgpa, c.package_lpa);
}

void list_companies(void) {
    if (!company_count) { printf("  No companies yet. Use 'addcompany'.\n"); return; }
    printf("\n  %-20s | %-8s | %-8s | %-11s | %s\n",
           "Company","Min CGPA","Pkg(LPA)","Backlogs","Branches");
    printf("  --------------------+----------+----------+-------------+------------------\n");
    for (int i = 0; i < company_count; i++) {
        Company *c = &companies[i]; char br[100]="";
        if (!c->branch_count) strcpy(br, "All");
        else for (int j = 0; j < c->branch_count; j++) {
            if (j>0) strcat(br,", "); strcat(br, c->allowed_branches[j]);
        }
        printf("  %-20s | %-8.2f | %-8.2f | %-11s | %s\n",
               c->name, c->min_cgpa, c->package_lpa,
               c->allow_backlogs ? "Allowed" : "Not allowed", br);
    }
    printf("\n  Total: %d company(s)\n", company_count);
}

/* Three-filter check: CGPA threshold + backlog rule + branch whitelist */
static int is_eligible(const Student *s, const Company *c) {
    if (s->cgpa < c->min_cgpa) return 0;
    if (!c->allow_backlogs && s->backlogs > 0) return 0;
    if (c->branch_count > 0) {
        char lb[MAX_BRANCH];
        strncpy(lb, s->branch, MAX_BRANCH-1); lb[MAX_BRANCH-1]='\0'; str_to_lower(lb);
        int ok = 0;
        for (int i = 0; i < c->branch_count; i++)
            if (strcmp(lb, c->allowed_branches[i]) == 0) { ok = 1; break; }
        if (!ok) return 0;
    }
    return 1;
}

void show_eligible(const char *co_name) {
    Company *found = NULL;
    for (int i = 0; i < company_count; i++)
        if (str_eq_nocase(companies[i].name, co_name)) { found = &companies[i]; break; }
    if (!found) { printf("  [Error] Company '%s' not found.\n", co_name); return; }

    printf("\n  Eligible for %s  (Min CGPA:%.2f | %.2f LPA | Backlogs:%s)\n",
           found->name, found->min_cgpa, found->package_lpa,
           found->allow_backlogs ? "Allowed" : "Not allowed");
    hdr_place(); int c = 0;
    for (int i = 0; i < student_count; i++)
        if (is_eligible(&students[i], found)) { row_place(&students[i]); c++; }
    sep_place();
    printf("  Total eligible: %d\n", c);
}

//  *  SECTION 15 — SKILL SEARCH
//  *  Linear scan over all students and their skill arrays.
//  *  O(n × max_skills) is fine at 1000 students — no fancy index needed.

void search_skill(const char *skill) {
    char lo[MAX_SKILL_LEN];
    strncpy(lo, skill, MAX_SKILL_LEN-1); lo[MAX_SKILL_LEN-1]='\0'; str_to_lower(lo);
    int found = 0; hdr_place();
    for (int i = 0; i < student_count; i++)
        for (int j = 0; j < students[i].skill_count; j++)
            if (strcmp(students[i].skills[j], lo) == 0) { row_place(&students[i]); found++; break; }
    sep_place();
    if (!found) printf("  No students with skill '%s'.\n", skill);
    else        printf("  Total: %d student(s) with skill '%s'.\n", found, skill);
}

//  *  SECTION 16 — EXPORT TO CSV
//  *  Dumps a filtered subset of students into export.csv.
//  *  Modes: all, placed, unplaced, cgpa>x, cgpa<x, branch=x

typedef enum { EXP_ALL, EXP_PLACED, EXP_UNPLACED, EXP_CGPA_GT, EXP_CGPA_LT, EXP_BRANCH } ExportMode;

void export_csv(ExportMode mode, float cgpa_val, const char *br_val) {
    FILE *f = fopen(EXPORT_FILE, "w");
    if (!f) { perror("Cannot create export.csv"); return; }
    fprintf(f, "ID,Name,Branch,CGPA,Backlogs,Status,Company,Package(LPA),Skills\n");
    int count = 0;
    for (int i = 0; i < student_count; i++) {
        Student *s = &students[i]; int inc = 0;
        switch (mode) {
            case EXP_ALL:      inc = 1; break;
            case EXP_PLACED:   inc = s->is_placed; break;
            case EXP_UNPLACED: inc = !s->is_placed; break;
            case EXP_CGPA_GT:  inc = (s->cgpa > cgpa_val); break;
            case EXP_CGPA_LT:  inc = (s->cgpa < cgpa_val); break;
            case EXP_BRANCH: {
                char lb[MAX_BRANCH];
                strncpy(lb, s->branch, MAX_BRANCH-1); lb[MAX_BRANCH-1]='\0'; str_to_lower(lb);
                inc = (strcmp(lb, br_val) == 0); break;
            }
        }
        if (inc) {
            char sk[200]="";
            for (int j = 0; j < s->skill_count; j++) { if (j>0) strcat(sk,"|"); strcat(sk, s->skills[j]); }
            if (!s->skill_count) strcpy(sk, "-");
            fprintf(f, "%d,%s,%s,%.2f,%d,%s,%s,%.2f,%s\n",
                    s->id, s->name, s->branch, s->cgpa, s->backlogs,
                    s->is_placed ? "Placed" : "Seeking",
                    s->is_placed ? s->company_placed_in : "-",
                    s->package_lpa, sk);
            count++;
        }
    }
    fclose(f);
    printf("  Exported %d record(s) to '%s'.\n", count, EXPORT_FILE);
}

//  *  SECTION 17 — STATISTICS DASHBOARD
//  *  Single pass over all students to collect every stat we need,
//  *  then prints it in a tidy box layout.

void show_stats(void) {
    if (!student_count) { printf("  No students in system.\n"); return; }

    int   placed=0, total_bl=0;
    float sum_cgpa=0, max_cgpa=-1, min_cgpa=11, sum_pkg=0, max_pkg=-1;
    char  top_name[MAX_NAME]="N/A";

    char br_name[200][MAX_BRANCH]; int br_cnt[200]; int br_n=0;
    char co_name[MAX_COMPANIES][MAX_CO_NAME]; int co_cnt[MAX_COMPANIES]; int co_n=0;

    for (int i = 0; i < student_count; i++) {
        Student *s = &students[i];
        sum_cgpa += s->cgpa; total_bl += s->backlogs;
        if (s->cgpa > max_cgpa) { max_cgpa = s->cgpa; strncpy(top_name, s->name, MAX_NAME-1); }
        if (s->cgpa < min_cgpa)   min_cgpa = s->cgpa;
        if (s->is_placed) {
            placed++; sum_pkg += s->package_lpa;
            if (s->package_lpa > max_pkg) max_pkg = s->package_lpa;
            int fnd=0;
            for (int j=0; j<co_n; j++)
                if (strcmp(co_name[j], s->company_placed_in)==0) { co_cnt[j]++; fnd=1; break; }
            if (!fnd && co_n < MAX_COMPANIES) {
                strncpy(co_name[co_n], s->company_placed_in, MAX_CO_NAME-1);
                co_cnt[co_n]=1; co_n++;
            }
        }
        char lb[MAX_BRANCH];
        strncpy(lb, s->branch, MAX_BRANCH-1); lb[MAX_BRANCH-1]='\0'; str_to_lower(lb);
        int fnd=0;
        for (int j=0; j<br_n; j++)
            if (strcmp(br_name[j], lb)==0) { br_cnt[j]++; fnd=1; break; }
        if (!fnd && br_n<200) { strncpy(br_name[br_n], lb, MAX_BRANCH-1); br_cnt[br_n]=1; br_n++; }
    }

    char top_br[MAX_BRANCH]="N/A"; int top_br_c=0;
    for (int i=0; i<br_n; i++)
        if (br_cnt[i]>top_br_c) { top_br_c=br_cnt[i]; strncpy(top_br, br_name[i], MAX_BRANCH-1); }

    char top_co[MAX_CO_NAME]="N/A"; int top_co_c=0;
    for (int i=0; i<co_n; i++)
        if (co_cnt[i]>top_co_c) { top_co_c=co_cnt[i]; strncpy(top_co, co_name[i], MAX_CO_NAME-1); }

    float avg_cgpa = sum_cgpa / (float)student_count;
    float avg_pkg  = placed > 0 ? sum_pkg / (float)placed : 0.0f;
    float pct      = ((float)placed / (float)student_count) * 100.0f;

    printf("\n");
    printf("  +=====================================================+\n");
    printf("  |        PLACEMENT STATISTICS DASHBOARD              |\n");
    printf("  +=====================================================+\n");
    printf("  |  Total Students   : %-5d                         |\n", student_count);
    printf("  |  Placed           : %-5d  (%.1f%%)                |\n", placed, pct);
    printf("  |  Unplaced         : %-5d                         |\n", student_count-placed);
    printf("  |  Total Backlogs   : %-5d                         |\n", total_bl);
    printf("  +-----------------------------------------------------+\n");
    printf("  |  CGPA STATS                                        |\n");
    printf("  |  Average CGPA     : %.2f                          |\n", avg_cgpa);
    printf("  |  Highest CGPA     : %.2f  (%s)\n",                      max_cgpa, top_name);
    printf("  |  Lowest CGPA      : %.2f                          |\n", min_cgpa);
    printf("  +-----------------------------------------------------+\n");
    printf("  |  PLACEMENT STATS                                   |\n");
    printf("  |  Avg Package      : %.2f LPA                      |\n", avg_pkg);
    printf("  |  Highest Package  : %.2f LPA                      |\n", max_pkg>0?max_pkg:0.0f);
    printf("  |  Top Company      : %s (%d students)\n",                top_co, top_co_c);
    printf("  +-----------------------------------------------------+\n");
    printf("  |  Top Branch       : %s (%d students)\n",                top_br, top_br_c);
    printf("  |  Registered Co.   : %-5d                         |\n", company_count);
    printf("  +=====================================================+\n");
}

//  *  SECTION 18 — QUERY PARSER
//  *  Reads a raw command string, figures out what was asked, and
//  *  routes to the right function. Field=value queries get split
//  *  at the operator character (>, <, =) to extract field + value.

static void handle_export(const char *rest) {
    if (!strlen(rest)||strcmp(rest,"all")==0) { export_csv(EXP_ALL,0,NULL);     return; }
    if (strcmp(rest,"placed")==0)             { export_csv(EXP_PLACED,0,NULL);  return; }
    if (strcmp(rest,"unplaced")==0)           { export_csv(EXP_UNPLACED,0,NULL);return; }
    char *op=NULL; char op_ch=0;
    for (int i=0; rest[i]; i++)
        if (rest[i]=='>'||rest[i]=='<'||rest[i]=='=') { op=(char*)rest+i; op_ch=rest[i]; break; }
    if (op) {
        char field[32]; int fl=(int)(op-rest);
        strncpy(field,rest,(size_t)fl); field[fl]='\0'; trim(field); str_to_lower(field);
        char val[128]; strncpy(val,op+1,sizeof(val)-1); val[sizeof(val)-1]='\0'; trim(val);
        if (strcmp(field,"cgpa")==0&&op_ch=='>') { export_csv(EXP_CGPA_GT,(float)atof(val),NULL); return; }
        if (strcmp(field,"cgpa")==0&&op_ch=='<') { export_csv(EXP_CGPA_LT,(float)atof(val),NULL); return; }
        if (strcmp(field,"branch")==0&&op_ch=='=') { str_to_lower(val); export_csv(EXP_BRANCH,0,val); return; }
    }
    printf("  [Error] Try: export all | placed | unplaced | cgpa>x | cgpa<x | branch=x\n");
}

void process_query(char *raw) {
    trim(raw);
    if (strcmp(raw,"exit")==0) { printf("\nGoodbye!\n"); exit(0); }

    /* reload: re-reads students.txt — handy if the file was edited externally */
    if (strcmp(raw,"reload")==0) {
        student_count=0;
        avl_free(avl_root); avl_root=NULL;
        trie_free(name_trie);   name_trie   = trie_new();
        trie_free(branch_trie); branch_trie = trie_new();
        heap_sz=0;
        load_students();
        printf("  [reload] %d student(s) loaded.\n", student_count);
        return;
    }

    if (strcmp(raw,"add")==0)        { printf("\n--- Add New Student ---\n"); add_student(); return; }
    if (strcmp(raw,"stats")==0)      { show_stats(); return; }
    if (strcmp(raw,"placed")==0)     { printf("\n  Placed Students:\n");   show_by_placement(1); return; }
    if (strcmp(raw,"unplaced")==0)   { printf("\n  Unplaced Students:\n"); show_by_placement(0); return; }
    if (strcmp(raw,"addcompany")==0) { printf("\n--- Add Company ---\n"); add_company(); return; }
    if (strcmp(raw,"companies")==0)  { printf("\n  Registered Companies:\n"); list_companies(); return; }

    if (strncmp(raw,"top ",4)==0) {
        int k = atoi(raw+4);
        if (k<=0) { printf("  [Error] Usage: top <number>\n"); return; }
        printf("\n  Top %d Students by CGPA:\n", k); heap_top_k(k); return;
    }
    if (strncmp(raw,"mark ",5)==0)   { mark_placement(raw+5); return; }
    if (strncmp(raw,"update ",7)==0) {
        int prn = atoi(raw+7);
        if (prn<=0) { printf("  [Error] Usage: update <prn>\n"); return; }
        printf("\n--- Update Student PRN %d ---\n", prn);
        update_student(prn); return;
    }
    if (strncmp(raw,"delete ",7)==0) {
        int prn = atoi(raw+7);
        if (prn<=0) { printf("  [Error] Usage: delete <prn>\n"); return; }
        delete_student(prn); return;
    }
    if (strncmp(raw,"eligible for ",13)==0) {
        char co[MAX_CO_NAME];
        strncpy(co, raw+13, MAX_CO_NAME-1); co[MAX_CO_NAME-1]='\0'; trim(co);
        if (!strlen(co)) { printf("  [Error] Specify company name.\n"); return; }
        show_eligible(co); return;
    }
    if (strncmp(raw,"export",6)==0) {
        char rest[256]; strncpy(rest, raw+6, sizeof(rest)-1); rest[sizeof(rest)-1]='\0'; trim(rest);
        handle_export(rest); return;
    }

    /* field op value — handles cgpa>, name=, branch=, skill= */
    char *op_ptr=NULL; char op_ch=0;
    for (int i=0; raw[i]; i++)
        if (raw[i]=='>'||raw[i]=='<'||raw[i]=='=') { op_ptr=raw+i; op_ch=raw[i]; break; }

    if (!op_ptr) {
        printf("  [Error] Unknown query: \"%s\"\n", raw);
        printf("  Hint: add | cgpa>x | cgpa<x | name=x | branch=x | skill=x | top k\n");
        printf("        placed | unplaced | mark id placed Co Pkg | mark id unplaced\n");
        printf("        update <prn> | delete <prn>\n");
        printf("        addcompany | companies | eligible for Co | export ... | stats | exit\n");
        return;
    }

    char field[32]; int fl=(int)(op_ptr-raw);
    if (fl<=0||fl>=(int)sizeof(field)) { printf("  [Error] Invalid query.\n"); return; }
    strncpy(field, raw, (size_t)fl); field[fl]='\0'; trim(field); str_to_lower(field);
    char value[128]; strncpy(value, op_ptr+1, sizeof(value)-1); value[sizeof(value)-1]='\0'; trim(value);
    if (!strlen(value)) { printf("  [Error] No value after operator.\n"); return; }

    if (strcmp(field,"cgpa")==0) {
        if (op_ch=='=') { printf("  [Error] Use cgpa > x or cgpa < x\n"); return; }
        float t = (float)atof(value);
        if (t<0||t>10) { printf("  [Error] CGPA must be 0.0-10.0\n"); return; }
        printf("\n  Students with CGPA %c %.2f:\n", op_ch, t);
        hdr_acad();
        if (op_ch=='>') avl_gt(avl_root, t); else avl_lt(avl_root, t);
        sep_acad();
    } else if (strcmp(field,"name")==0) {
        if (op_ch!='=') { printf("  [Error] Use name = x\n"); return; }
        str_to_lower(value);
        printf("\n  Students with name = \"%s\":\n", value);
        hdr_acad(); int n=trie_search(name_trie, value); sep_acad();
        if (!n) printf("  Not found.\n");
    } else if (strcmp(field,"branch")==0) {
        if (op_ch!='=') { printf("  [Error] Use branch = x\n"); return; }
        str_to_lower(value);
        printf("\n  Students in branch \"%s\":\n", value);
        hdr_acad(); int n=trie_search(branch_trie, value); sep_acad();
        if (!n) printf("  Not found.\n");
    } else if (strcmp(field,"skill")==0) {
        if (op_ch!='=') { printf("  [Error] Use skill = x\n"); return; }
        printf("\n  Students with skill = \"%s\":\n", value);
        search_skill(value);
    } else {
        printf("  [Error] Unknown field '%s'. Use: cgpa, name, branch, skill\n", field);
    }
}

//  *  SECTION 19 — MAIN
//  *  Boots up the system, loads saved data, then sits in a read-
//  *  eval loop until the user types "exit".

int main(void) {
    name_trie = trie_new(); branch_trie = trie_new();
    load_students(); load_companies();

    printf("\n");
    printf("  +========================================================+\n");
    printf("  |   STUDENT PLACEMENT MANAGEMENT SYSTEM  v2.0  (SPMS)   |\n");
    printf("  +========================================================+\n");
    printf("  Loaded: %d student(s) | %d company(s)\n\n", student_count, company_count);
    printf("  ACADEMIC    : add | cgpa>x | cgpa<x | name=x | branch=x | skill=x | top k\n");
    printf("  MANAGE      : update <prn> | delete <prn>\n");
    printf("  PLACEMENT   : placed | unplaced | mark id placed Co Pkg | mark id unplaced\n");
    printf("  COMPANY     : addcompany | companies | eligible for <company>\n");
    printf("  EXPORT      : export all | export placed | export cgpa>8 | export branch=cs\n");
    printf("  DASHBOARD   : stats\n");
    printf("  QUIT        : exit\n\n");

    char query[256];
    while (1) {
        printf("  >> "); fflush(stdout);
        if (!fgets(query, sizeof(query), stdin)) break;
        char tmp[256]; strncpy(tmp, query, sizeof(tmp)-1); tmp[sizeof(tmp)-1]='\0'; trim(tmp);
        if (!strlen(tmp)) continue;
        process_query(query); printf("\n");
    }
    return 0;
}