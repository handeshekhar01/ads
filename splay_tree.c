#include <stdio.h>
#include <stdlib.h>

struct Node {
    int key;
    struct Node *left, *right;
};

struct Node* newNode(int key) {
    struct Node* node = (struct Node*)malloc(sizeof(struct Node));
    node->key = key;
    node->left = node->right = NULL;
    return node;
}

struct Node *rightRotate(struct Node *x) {
    struct Node *y = x->left;
    x->left = y->right;
    y->right = x;
    return y;
}

struct Node *leftRotate(struct Node *x) {
    struct Node *y = x->right;
    x->right = y->left;
    y->left = x;
    return y;
}

struct Node *splay(struct Node *root, int key) {
    if (root == NULL || root->key == key)
        return root;

    if (root->key > key) {
        if (root->left == NULL) return root;

        if (root->left->key > key) {
            root->left->left = splay(root->left->left, key);
            root = rightRotate(root);
        }
        else if (root->left->key < key) {
            root->left->right = splay(root->left->right, key);
            if (root->left->right != NULL)
                root->left = leftRotate(root->left);
        }
        return (root->left == NULL) ? root : rightRotate(root);
    } 
    else {
        if (root->right == NULL) return root;

        if (root->right->key > key) {
            root->right->left = splay(root->right->left, key);
            if (root->right->left != NULL)
                root->right = rightRotate(root->right);
        }
        else if (root->right->key < key) {
            root->right->right = splay(root->right->right, key);
            root = leftRotate(root);
        }
        return (root->right == NULL) ? root : leftRotate(root);
    }
}

struct Node *insert(struct Node *root, int k) {
    if (root == NULL) return newNode(k);

    root = splay(root, k);

    if (root->key == k) return root;

    struct Node *newnode = newNode(k);
    if (root->key > k) {
        newnode->right = root;
        newnode->left = root->left;
        root->left = NULL;
    } else {
        newnode->left = root;
        newnode->right = root->right;
        root->right = NULL;
    }
    return newnode;
}

// Function to print tree in Preorder (Root, Left, Right)
void preOrder(struct Node *root) {
    if (root != NULL) {
        printf("%d ", root->key);
        preOrder(root->left);
        preOrder(root->right);
    }
}

int main() {
    struct Node *root = NULL;
    int choice, value;

    printf("--- Splay Tree Implementation ---\n");

    while(1) {
        printf("\n1. Insert\n2. Search/Splay\n3. Display (Preorder)\n4. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch(choice) {
            case 1:
                printf("Enter value to insert: ");
                scanf("%d", &value);
                root = insert(root, value);
                printf("%d inserted and splayed to root.\n", value);
                break;
            case 2:
                if (root == NULL) {
                    printf("Tree is empty!\n");
                } else {
                    printf("Enter value to search: ");
                    scanf("%d", &value);
                    root = splay(root, value);
                    if (root->key == value)
                        printf("Found %d. It is now at the root.\n", value);
                    else
                        printf("Value not found. Closest node %d splayed to root.\n", root->key);
                }
                break;
            case 3:
                printf("Current Tree (Preorder): ");
                preOrder(root);
                printf("\n");
                break;
            case 4:
                exit(0);
            default:
                printf("Invalid choice!\n");
        }
    }
    return 0;
}