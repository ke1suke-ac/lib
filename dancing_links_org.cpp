#include <bits/stdc++.h>

namespace linked_matrix_GJK
{

template<class T>
class MNode_t;

class Column;

//! Identifies location of a node within a matrix
class MData
{
public:
    MData(int r=-1, Column *c=NULL) : row_id(r), column_id(c) {}
    int row_id;  //!< A number identifying the row
    Column *column_id; //!< A pointer to the column header
};

//! The default matrix node used in this project.
typedef MNode_t<MData> MNode;


/**
 * \brief Implementation of a matrix as a linked data structure.
 * 
 * An efficient implementation of a boolean matrix, used in a solution of the Exact Cover Problem.
 * This implementation only stores non-zero entries, and is especially suitable for sparse matrices.
 * Moreover, rows and columns are represented as circular doubly linked lists, so as to be naturally
 * amenable to the so-called ``Dancing Links'' algorithm of D. Knuth. 
 * 
 * The following is a helpful illustration of the LMatrix data structure taken from [this article](https://arxiv.org/pdf/cs/0011047.pdf) by Knuth.
 * ![An image of a linked matrix](../images/linked_matrix_image_(Knuth).png)
 * 
 * The data structure consists of a *root* or *head* node (got by calling \ref head() ) which is linked to the
 * *column headers* (objects of type \ref Column ), each of which represents a column of the matrix and whose
 * \ref Column#size() records the number of 1's in that column. Each 1 is represented by an object of type
 * \ref MNode, and each row and column (including the header) is a circular doubly linked list.
 * 
 * For each node, \ref MData#column_id points to the column header, and \ref MData#row_id is the row index.  The
 * values of the row index are determined by the matrix passed as an argument to the constructor - gaps in the
 * row index values of an \ref LMatrix reflect zero rows in the original matrix.  For
 * column headers, \ref MData#row_id = -1.  Also for \ref head(), \ref MData#column_id = @c NULL.  
 * 
 * @see dancing_links.h, \ref dancing_links_GJK::Exact_Cover_Solver
 */
class LMatrix
{
public:
    LMatrix(); //!< Creates an empty matrix.
    /**
     * @brief Converts a standard boolean matrix to \ref LMatrix form.
     * @param matrix A boolean matrix.
     * @param m The number of rows in @p matrix.
     * @param n The number of columns in @p matrix.
     */
    LMatrix(bool **matrix, int m, int n);
    MNode* head() const; //!< \return the head node of the matrix (see the detailed class description).
    bool is_trivial() const; //!< \return 1 if the matrix is empty (ie consists only of a head node), 0 otherwise.
    int number_of_rows() const; //!< \return the number of rows (equivalently, the maximum column size).
    /**
     * @brief Removes the row containing the node @p node.
     * @param node A node of the matrix.
     * 
     * Does not alter any left, right, up or down links belonging to nodes in the row.  If the matrix is empty 
     * or @p node is \ref head() or a column header, no action is taken.
     * 
     * \ref Column#size() values are updated, but \ref row_id values are **not** changed.
     */
    void remove_row(MNode *node);
    /**
     * @brief Undoes the action of \ref LMatrix#remove_row "remove_row"( @p node).
     * 
     * **Precondition** Neither the row containing @c node, nor the calling object have been altered since the
     * last call to \ref remove_row.
     */
    void restore_row(MNode *node);
    /**
     * @brief Removes the column containing the node @p node.
     * @param node A node of the matrix.
     * 
     * Removes the entire column, including the column header.  Does not alter any left, right, up or down links belonging to nodes in the column.  If @p node is \ref head(),
     * no action is taken.
     */
    void remove_column(MNode *node);
    /**
     * @brief Undoes the action of \ref LMatrix#remove_column "remove_column"( @p node).
     * 
     * **Precondition** Neither the column containing @c node, nor the calling object have been altered since the
     * last call to \ref remove_column.
     */
    void restore_column(MNode *node);
    ~LMatrix();
    /**
 * @brief Displays the matrix in ascii format, which is useful for debugging.
 * @param out_stream The output stream of the display.
 * 
 * Each line of the display corresponds to a row of the matrix and ends with the row number.  Empty rows are included.
 * The head <tt>M.head()</tt> is represented by the symbol `H', column headers by the symbol `C', and all
 * other nodes (ie non-zero matrix entries) by `N'.  Left and right links are represented by the symbols 
 * `<' and `>' respectively, 
 * and each row is understood to wrap into a circular list.
 * 
 * As an example, the matrix
 * 
        1 0 1
        0 0 0
        0 1 1
 * 
 * when converted to type LMatrix, will be displayed as
 *       
         ROW DIAGRAM
         
         >H<>C<>C<>C<    row -1
            >N<   >N<    row 0
                         row 1
               >N<>N<    row 2

 * 
 * Multiple @c assert statements will be made during the process of creating the display.  These
 * will check the structure of the matrix (including the up/down links, which are not displayed).
 * 
 * Following the display of the matrix, the column sizes will be displayed in sequence.  For the above example,

         COLUMN SIZES
         
         >1<>1<>2<
          
will be displayed.  This can be used to check that the \ref Column#size() values are correct.
 */
    void DEBUG_display(std::ostream& out_stream=std::cout);
private:
    MNode *root;
    int row_count;      // 1 plus the index of the last nonzero row index of the ORIGINAL matrix
                        // set in the constructor and not ever modified
                        // only needed for DEBUG_display()
                        // note: if many rows have been deleted from the matrix, this could well be unnecessarily large.
};



/**
 * \brief A node containing data of variable type @c T and four links to other nodes - left, right, up and down.
 *
 */
template<class T>
class MNode_t
{
public:
    MNode_t(const T& theData, MNode_t<T> *theRight = NULL, MNode_t<T> *theLeft = NULL,
          MNode_t<T> *theUp = NULL, MNode_t<T> *theDown = NULL) 
          : _data(theData), right_link(theRight), left_link(theLeft), up_link(theUp), down_link(theDown) {}
    void set_data(const T& theData) {_data = theData;} //!< Sets the node's data to @p theData.
    T& data() const {return const_cast<T&>(_data);} //!< \return the data object. 
    // don't know why I have to cast the argument, but compiler otherwise complains
    MNode_t<T> * right() const {return right_link;} //!< \return the right link.
    MNode_t<T> * left() const {return left_link;} //!< \return the left link.
    MNode_t<T> * up() const {return up_link;} //!< \return the up link.
    MNode_t<T> * down() const {return down_link;} //!< \return the down link.
    void set_right(MNode_t<T> *node) {right_link = node;} //!< Points the right link to @c node.
    void set_left(MNode_t<T> *node) {left_link = node;} //!< Points the left link to @c node.
    void set_up(MNode_t<T> *node) {up_link = node;} //!< Points the up link to @c node.
    void set_down(MNode_t<T> *node) {down_link = node;} //!< Points the down link to @c node.
private:
    T _data;
    MNode_t<T> *right_link;
    MNode_t<T> *left_link;
    MNode_t<T> *up_link;
    MNode_t<T> *down_link;
};


/*!
 * \brief A type of \ref MNode which acts as a header for a column
 *
 * The \ref MData#row_id is set to -1 and \ref MData#column_id points to the object itself.
 * \ref Column objects also store the size of their column, which can be accessed via member functions.
 */
class Column : public MNode
{
public:
    Column(int theSize=0) 
        : MNode(MData(-1,this)), _size(theSize) {}
    int size() const {return _size;} //!< returns the number of nodes in the column.
    void set_size(int N) {_size = N;} //!< sets the number of nodes in the column to @p N.
    void add_to_size(int N) {_size += N;} //!< Adds @p N to the number of nodes in the column.
private:
    int _size;
};


/**
 * @brief Joins two nodes together horizontally.
 * 
 * Sets the right link of @p a to @p b and the left link of @p b to @p a.
 * Assumes neither @p a nor @p b point to @c NULL.
 */
template<class T>
void join_lr(MNode_t<T> *a, MNode_t<T> *b);

/**
 * @brief Joins two nodes together vertically.
 * 
 * Sets the up link of @p a to @p b and the down link of @p b to @p a.
 * Assumes neither @p a nor @p b point to @c NULL.
 */
template<class T>
void join_du(MNode_t<T> *a, MNode_t<T> *b);


} // end namespace 'linked_matrix_GJK'

#include <iostream>   // for 'DEBUG_display()'
#include <cassert>    // for 'DEBUG_display()'

using std::endl;

namespace linked_matrix_GJK
{

/*****************************************************************************************************
 * implementation of class LMatrix
 */
    
LMatrix::LMatrix(void) : root( new MNode( MData() ) )
{   
    // make root->down_link constant somehow
    join_lr(root, root);
    row_count = 0;
}

LMatrix::LMatrix(bool **matrix, int m, int n) : root( new MNode( MData() ) )
{   
    if( m == 0 || n == 0 ) {
        row_count = 0;
        return;
    }
    // create first column
    MNode *c = new Column(0);
    //c->data().column_id = static_cast<Column*>(c);        // point column object to itself
    join_lr(root,c);
    // create column header objects
    for(int j = 1; j < n; j++) {
        join_lr(c, new Column(0) );
        c = c->right();
        //c->data().column_id = static_cast<Column*>(c);
    }
    join_lr(c,root);
    
    // initialize m x n array of MNode pointers
    MNode ***ptr_matrix = new MNode**[m];
    for(int k = 0; k < m; k++) {
        ptr_matrix[k] = new MNode*[n];
    }
    
    // create nodes of LMatrix, referenced by pointers in ptr_matrix
    // also link the nodes vertically
    MNode *tmp;
    c = root->right();
    // j = column of matrix, i = row of matrix
    for( int j = 0; j < n; j++, c = c->right() ) {
        tmp = c;
        for(int i = 0; i < m; i++) {
            if(matrix[i][j]) {
                ptr_matrix[i][j] = new MNode(MData(i,static_cast<Column*>(c)));
                join_du(ptr_matrix[i][j], tmp);
                tmp = ptr_matrix[i][j];
                (static_cast<Column *>(c))->add_to_size(1);
            } 
            else ptr_matrix[i][j] = NULL;
        }
        join_du(c, tmp);
    }
    
    // ignore zero rows at the bottom of the matrix
    int i;
    bool zero_row;
    for(i=m-1; i >= 0; i--) {
        zero_row = 1;
        for(int j = 0; j < n; j++) {
            if(matrix[i][j]) {
                zero_row = 0;
                break;
            }
        }
        if(!zero_row) break;
    }
    
    // 'i' is now the index of the last non-zero row, or -1 if there are no non-zero rows
    row_count = i + 1;

    
    // link the nodes horizontally
    MNode * first, *prev;
    // i = row, j = column
    for(; i >= 0; i--) {
        first = NULL;
        for(int j = 0; j < n; j++ ) {
            // find first non-zero matrix entry in row i,
            // and make 'first' point to the corresponding node
            if(ptr_matrix[i][j] != NULL) {
                if( first == NULL) {
                    first = ptr_matrix[i][j];
                } else {
                    join_lr(prev, ptr_matrix[i][j]);
                }
                prev = ptr_matrix[i][j];
            }
        }
        if(first != NULL) {  // if row i is not a zero row
            join_lr(prev, first);
        }
    }
    
    // finished with ptr_matrix, so now delete it
    // note this doesn't delete the nodes of the new LMatrix, just their pointers which 
    // are stored in ptr_matrix
    for(int k = 0; k < m; k++) {
        delete[] ptr_matrix[k];
    }
    delete[] ptr_matrix;
    
}

MNode* LMatrix::head() const
{
    return root;
}

bool LMatrix::is_trivial() const
{
    return root->right() == root && root->left() == root;
}

int LMatrix::number_of_rows() const
{
    int num = 0;
    for(MNode *node = root->right(); node != root; node = node->right() ) {
        if(static_cast<Column*>(node)->size() > num) num = static_cast<Column*>(node)->size();
    }
    return num;
}


void LMatrix::remove_row(MNode * node)
{   
    if(node == NULL || node == root || node->data().column_id == node ) return;
    MNode *k = node;
    do {
        join_du( k->down(), k->up() ); 
        k->data().column_id->add_to_size(-1);
        k = k->right();
    } while( k != node ); // stop when we're back where we started
}



void LMatrix::restore_row(MNode * node)
{
    MNode *k = node;
    do {
        k->up()->set_down(k);  // connect row back
        k->down()->set_up(k);  // into the matrix
        k->data().column_id->add_to_size(1);
        k = k->left();
    } while( k != node );
}




void LMatrix::remove_column(MNode * node)
{   
    if(node == NULL || node == root ) return;
    MNode *k = node;
    do {
        join_lr( k->left(), k->right() ); 
        k = k->up();
    } while( k != node ); // stop when we're back where we started
}


void LMatrix::restore_column(MNode * node)
{
    MNode *k = node;
    do {
        k->right()->set_left(k);
        k->left()->set_right(k);
        k = k->down();
    } while( k != node );
}


LMatrix::~LMatrix()
{
    MNode *a, *b, *del;
    // a iterates through the column headers horizontally
    // b iterates through each column vertically
    a = root->right();
    while(a != root) {
        b = a->down();
        while(b != a) {
            del = b;
            b = b->down();
            delete del;
        }
        del = a;
        a = a->right();
        delete del;
    }
    delete root;
}



void LMatrix::DEBUG_display(std::ostream& ofs)
{   
    const char l = '>';
    const char d = '<';
    const char r = '<';
    const char u = '>';
    const char H = 'H';
    const char C = 'C';
    const char N = 'N';
    const char ind = '\t';
    const char sp = ' ';
    ofs << ind << "ROW DIAGRAM" << endl << endl;
    
    ofs << ind;
    ofs << l << H << r;

    MNode *node = root->right();
    assert( node->left() == root );
    while( node != root ) {
        assert( node->data().row_id == -1 );
        assert( node->right() != NULL );
        assert( node == node->right()->left() );
        ofs << l << C << r;
        node = node->right();
    }
    ofs << ind << ind << "row " << -1 << endl;
    
    int rownum = 0;
    MNode *colhead;
    MNode *prev, *first;
    while(rownum < row_count ) {
        ofs << ind << sp << sp << sp;
        colhead = root->right();
        prev = NULL;
        while(colhead != root) {
            node = colhead->down();
            while(node != colhead && node->data().row_id != rownum) {
                assert(node != NULL);
                assert(node->down() != NULL);
                assert(node->down()->up() == node);
                assert(node->data().column_id == colhead);
                node = node->down();
            }
            if(node == colhead) {
                ofs << sp << sp << sp;
            } else if(node->data().row_id == rownum) {
                if(prev != NULL) {
                    assert(prev->right() == node);
                    assert(node->left() == prev);
                } else {
                    first = node;
                }
                ofs << l << N << r;
                prev = node;
            }
            colhead = colhead->right();
        }
        if(prev != NULL) {
            assert(prev->right() == first);
            assert(first->left() == prev);
        }
        ofs << ind << ind << "row " << rownum << endl;
        rownum++;
    }
    
    ofs << endl << endl;
    ofs << ind << "COLUMN SIZES" << endl << endl;
    ofs << ind;
    node =root->right();
    while(node != root) {
        ofs << l << static_cast<Column*>(node)->size() << r;
        node = node->right();
    }
    ofs << endl;
}


/*****************************************************************************************************
 * implementation of MNode_t operations
 */

template<class T>
void join_lr(MNode_t<T> *a, MNode_t<T> *b)
{   
    a->set_right(b);
    b->set_left(a);
}

template<class T>
void join_du(MNode_t<T> *a, MNode_t<T> *b)
{
    a->set_up(b);
    b->set_down(a);
}


}

namespace dancing_links_GJK
{

using linked_matrix_GJK::MNode;
using linked_matrix_GJK::LMatrix;
using linked_matrix_GJK::Column;

typedef std::vector<int> S_Stack;


enum class RC {row, column};
/**
 \brief Holds the data of a row or column removal (of a matrix determined by context).
 
        Whether a row or column was removed is determined by \ref RC_Item#type.
        The field \ref RC_Item#node records the node which was passed to \ref LMatrix#remove_row or \ref LMatrix#remove_column.
 */
struct RC_Item {
    MNode* node;
    RC type;
};

//! Each \ref RC_Stack is to hold the data accumulated by a single call to \ref update().
typedef std::stack<RC_Item> RC_Stack;
/**
    \brief A class used to represent the history of a solution finding process (depth first search).

 */
typedef std::stack<RC_Stack> H_Stack;


/**
 * @brief Solves the exact cover problem for a boolean matrix @p matrix.
 * @param matrix A boolean matrix.
 * @param m The number of rows.
 * @param n The number of columns.
 * @return a solution of the exact cover problem in the form of a vector of row indices.
 * 
 * Converts @p matrix to an \ref LMatrix object and calls \ref DLX().
 * 
*/
std::vector<int> Exact_Cover_Solver(bool **matrix, int m, int n);

/**
 * @brief Like \ref dancing_links_GJK#Exact_Cover_Solver(bool**,int,int) but without the initial conversion
 * step. 
 */
std::vector<int> Exact_Cover_Solver(LMatrix& M);

/**
 * @brief Recursively solves the exact cover problem using the *dancing links* algorithm of Donald Knuth.
 * @param M The matrix.  It is modified during execution.  If a solution is found, @p M will be the empty matrix
 *            at termination, otherwise it will be unmodified.
 * @param solution At termination, holds the row indices of a solution
 *                  if one exists, otherwise is unmodified.
 * @param history At termination, holds the sequence of row and column deletions
 *                  traversed in finding the solution if one exists, otherwise is unmodified.  See \ref H_Stack.
 * @return 1 if a solution exists, 0 otherwise.
 * 
 * 
 * This procedure implements **Algorithm X**, an obvious backtracking algorithm which can be found on [this wikipedia page](https://en.wikipedia.org/wiki/Knuth%27s_Algorithm_X).
 * In pseudocode:
 * 
 *       If the matrix A has no columns, the current partial solution is a valid solution; terminate successfully.
         Otherwise choose a column c (deterministically).
         Choose a row r such that Ar, c = 1 (nondeterministically).
         Include row r in the partial solution.
         For each column j such that Ar, j = 1,
             for each row i such that Ai, j = 1,
                 delete row i from matrix A.
             delete column j from matrix A.
         Repeat this algorithm recursively on the reduced matrix A
 */
bool DLX(LMatrix& M, S_Stack& solution, H_Stack& history);


/**
 * @brief Column selector.
 * @param M a matrix.
 * @return a pointer to the column of @p M with the fewest nodes (ie smallest \ref Column#size() value).  If @p M is empty, return @c NULL.
 */
Column* choose_column(LMatrix& M);


/**
 * @brief Uses @p r to branch into the next step of Algorithm X, deleting the appropriate rows and columns of @p M.
 * @param M a matrix.
 * @param solution The deleted columns are pushed onto the back of this vector.
 * @param history The deleted rows and columns are pushed onto this stack.
 * @param r Indicates the row for the next branch.
 */
void update(LMatrix& M, S_Stack& solution, H_Stack& history, MNode *r);


/**
 * @brief Assumes \ref update() has just been called, and reverses the actions performed by \ref update().
 *
 * It is *very important* that neither @p M, @p solution, nor @p history have been modified since the last call
 * to \ref update().  If this condition is not met the behaviour is undefined.
 * 
 */
void downdate(LMatrix& M, S_Stack& solution, H_Stack& history);
    
    
}

#include <vector>
#include <stack>
#include <iostream> // required for "linked_matrix.h"

namespace dancing_links_GJK
{

/* 
 *  an implementation of Donald Knuth's Dancing Links algorithm
 *  https://arxiv.org/pdf/cs/0011047.pdf
 *  https://en.wikipedia.org/wiki/Dancing_Links
 */




std::vector<int> Exact_Cover_Solver(bool **matrix, int m, int n)
{   
    LMatrix M(matrix, m, n);
    H_Stack history;
    std::vector<int> solution;
    solution.reserve(m);
    DLX(M, solution, history);
    solution.shrink_to_fit();
    return solution;
}

std::vector<int> Exact_Cover_Solver(LMatrix& M)
{   
    H_Stack history;
    std::vector<int> solution;
    solution.reserve(M.number_of_rows());
    DLX(M, solution, history);
    solution.shrink_to_fit();
    return solution;
}

 
 
bool DLX(LMatrix& M, S_Stack& solution, H_Stack& history)
{
    Column *c = choose_column(M);
    // 'M' is empty => solution successfully found
    if( c == NULL ) { 
        return 1;
    }
    for( MNode *r = c->down(); r != static_cast<MNode*>(c); r = r->down() ) {
        update(M, solution, history, r);
        if( DLX(M, solution, history) ) { 
            return 1;     
        } else {
            downdate(M, solution, history);            
        }
    }
    // no solution exists
    return 0;
}

/* Given a matrix of linked nodes M, return a pointer to the column with the fewest nodes
 * If there are no columns, return NULL
 */
Column* choose_column(LMatrix& M)
{
    if( M.is_trivial() ) return NULL;
    
    Column* col = static_cast<Column*>(M.head()->right());
    Column* max_col = col;

    while( col != M.head() )
    {   
        if( col->size() < max_col->size() ) {
            max_col = col;
        }
        col = static_cast<Column*>(col->right());
    }
    return max_col;
}

/*
 * 
 */
void update(LMatrix& M, S_Stack& solution, H_Stack& history, MNode *r) 
{
    solution.push_back(r->data().row_id);
    
    RC_Stack temp_stack;
    RC_Item temp_item;

    for(MNode * i = r->right(); i != r; i = i->right()) {
        for(MNode *j = i->up(); j != i; j = j->up() ) {
            if(j->data().column_id == j) continue;
            M.remove_row(j);
            temp_item.node = j;
            temp_item.type = RC::row;
            temp_stack.push(temp_item);
        }
        M.remove_column(i);
        temp_item.node = i;
        temp_item.type = RC::column;
        temp_stack.push(temp_item);
    }
    
    for(MNode *j = r->up(); j != r; j = j->up() ) {
        if(j->data().column_id == j) continue;
        M.remove_row(j);
        temp_item.node = j;
        temp_item.type = RC::row;
        temp_stack.push(temp_item);
    }
    M.remove_column(r);
    temp_item.node = r;
    temp_item.type = RC::column;
    temp_stack.push(temp_item);
    
    history.push(temp_stack);
}


/*
 * Undoes the operations of 'update'
 */
void downdate(LMatrix& M, S_Stack& solution, H_Stack& history) 
{
    if( history.empty() ) {
        return;
    }
    solution.pop_back();
    RC_Stack last = history.top();
    RC_Item it;
    while( !last.empty() ) {
        it = last.top();
        if( it.type == RC::row ) {
            M.restore_row(it.node);
        } else if( it.type == RC::column ) {
            M.restore_column(it.node);
        }
        last.pop();
    }
    history.pop();
}


}




















namespace sudoku_GJK
{


/** \brief Wrapper class for @c int's in the range <tt>0 . . size</tt>
 * 
 * Is used to represent row indices, column indices, and values in a sudoku.
 */
template<int size>
class num_t {
public: 
    num_t(int n=0) : _n(n) { assert(0<=n && n<=size); } //!< Converts @c int to \ref num_t, checking that number is in range
    bool is_blank() const {return _n == 0;}  //!< Whether the object represents a blank/undetermined value (=0)
    operator int() const { return _n; }     //!< Operator for conversion to @c int
    num_t & operator=(const num_t & rhs) { _n = rhs._n; return *this;}
private:
    int _n;
};


/**
 * @brief Represents a sudoku triple.
 */
template<int sqrt_of_size>
struct Triple {
private:
    static const int size = sqrt_of_size*sqrt_of_size;
public:
    num_t<size> row;
    num_t<size> column;
    num_t<size> value;
    Triple(int r, int c, int v) : row(num_t<size>(r)), column(num_t<size>(c)), value(num_t<size>(v)) {};
    int to_index();
    /**
     * @brief Inverse of function \ref to_index(). 
     */
    Triple(int index);
/** \brief  Returns 1 if the triple belongs to @p square
 *      
 * Squares are numbered from left to right and descending. 
 * So @p square = @c s = @c a + @c b &times; @c sqrt_of_size, where @c a and @c b are illustrated
 * in the example below.
 *
 *               EXAMPLE.
 *      sqrt_of_size = 3, size = 9
 * 
 *          a = 1, 2, 3
 *      ___________________
 *      | s=1 | s=2 |  3  |     b
 *      |_____|_____|_____|     =
 *      |  4  |  5  |  6  |     0,
 *      |_____|_____|_____|     1,
 *      |  7  |  8  |  9  |     2
 *      |_____|_____|_____|
 *
 * 
 *      Which square is the triple (4,6,9) in?
 *      a = 2, b = 1
 *      => s = 2 + 3*1 = 5
 * 
 */
    bool is_in_square(num_t<size> square);
};


/**
 * @brief Represents a sudoku puzzle.
 */
template<int sqrt_of_size>
class Sudoku {
public:
    Sudoku();
    //Sudoku(int s, Iterable data);
    /**
     * @brief Only for 9x9 sudokus ( @c sqrt_of_size = 3).
     * @param flat_grid_str A C string representing the sudoku grid in row contiguous format.
     * 
     * \todo add constructor for sizes other than 9 (not necessarily with string input)
     */
    Sudoku(char *flat_grid_str);
    //! \returns the entry at position `(row,col)` in the sudoku grid.
    num_t<sqrt_of_size*sqrt_of_size> get_entry(
                                                num_t<sqrt_of_size*sqrt_of_size> row, 
                                                num_t<sqrt_of_size*sqrt_of_size> col) 
                                    { return grid[row-1][col-1]; }
    //! Writes the data in @p triple to the sudoku grid.
    void write(Triple<sqrt_of_size> triple);      //!< \todo change to array Triple<sqrt_of_size>[]
    bool found(Triple<sqrt_of_size> triple);      //!< \return 1 if @p triple is found, 0 otherwise
    //! Equality of cell values.
    bool operator==(const Sudoku<sqrt_of_size>& other) {
        for(int i=0;i<9;i++) {
            for(int j=0;j<9;j++) {
                if( (int)grid[i][j] != (int)other.grid[i][j]) return false;
            }
        } return true;
    }
    /** \brief Perform logical operations to partially solve a sudoku
     * 
     * \todo Implement this method.
     */
    void logic_solve();
/**
 * @brief Solves the sudoku puzzle
 *
 * Converts the sudoku to an Exact Cover Problem represented by a \ref linked_matrix_GJK::LMatrix
 * and calls \ref dancing_links_GJK::Exact_Cover_Solver(linked_matrix_GJK::LMatrix&).
 */
    void solve();
/**
 \brief Displays the sudoku using ASCII characters.

The format is demonstrated in the following sample output:

```
        =========================
        |       |   5 8 | 1 7   |
        |       |     4 |       |
        | 8   7 | 1     |       |
        =========================
        | 7 5   |     6 | 9 3 1 |
        |       | 9   7 |     5 |
        |   9   |   8 1 | 2     |
        =========================
        | 5 7   |   6   |   9 3 |
        |       |       | 7   8 |
        |     4 | 3 7   | 5     |
        =========================
```

     */
    void display_ASCII() const;
    ~Sudoku();
private:
    static const int size = sqrt_of_size*sqrt_of_size;
    num_t<size> **grid;
    void get_ECP_matrix(bool** matrix);
    void convert_ECP_solution(std::vector<int> solution);
};


} //end namespace 'sudoku_GJK'

namespace dancing_links_GJK {
    std::vector<int> Exact_Cover_Solver(linked_matrix_GJK::LMatrix&);
}

namespace sudoku_GJK
{

template<int sqrt_of_size>
Sudoku<sqrt_of_size>::Sudoku() 
{
    int j = 0;
    grid = new num_t<size>*[size];
    for(int i = 0; i < size; i++) {
        grid[i] = new num_t<size>[size];
    }
}

template<int sqrt_of_size>
Sudoku<sqrt_of_size>::Sudoku(char* flat_grid_str) : Sudoku()
{   
    assert(sqrt_of_size==3);
    int i,j;
    int k = 0;
    while(flat_grid_str[k] != '\0') {
        std::div_t dv = std::div(k,size);
        j = dv.rem;  // column
        i = dv.quot; // row
        grid[i][j] = num_t<size>(flat_grid_str[k] - '0'); // IMPORTANT that size == 9
        assert(0 <= grid[i][j] && grid[i][j] <= 9);     // utilizes num_t's int conversion operator
        k++;
    }
    assert(k==81);
}

template<int sqrt_of_size>
Sudoku<sqrt_of_size>::~Sudoku()
{
    for(int i = 0; i < size; i++) {
        delete[] grid[i];
    }
    delete[] grid;
}

template<int sqrt_of_size>
void Sudoku<sqrt_of_size>::logic_solve()
{
    // implement this method
}

template<int sqrt_of_size>
void Sudoku<sqrt_of_size>::solve()
{
    int m = size*size*size;     // # rows of matrix for the exact cover problem formulation
    int n = 4*size*size;        // # columns         ''              ''                  ''
    bool **matrix = new bool*[m];
    for(int i=0; i < m; i++) {
        matrix[i] = new bool[n];
    }
    get_ECP_matrix(matrix);
    linked_matrix_GJK::LMatrix M(matrix,m,n);
    // free memory
    for(int i=0; i < m; i++) {
        delete[] matrix[i];
    }
    delete[] matrix;

    std::vector<int> solution = dancing_links_GJK::Exact_Cover_Solver(M);
    // exception handling?
    
    convert_ECP_solution(solution);
    
}

template<int sqrt_of_size>
void Sudoku<sqrt_of_size>::write(Triple<sqrt_of_size> triple)
{
    grid[triple.row-1][triple.column-1] = triple.value;
}

template<int sqrt_of_size>
bool Sudoku<sqrt_of_size>::found(Triple<sqrt_of_size> triple)
{
    return grid[triple.row-1][triple.column-1] == triple.value;
}




/*
 * 
 *            ________row conditions_________column_______square________cell conditions_____
 *            |                          |            |            |                       |
 *          j | (r1,1) (r1,2) ... (r9,9) | (c1,1) ... | (s1,1) ... | (1,1) (1,2) ... (9,9) |
 *     i
 *  (1,1,1)       1      0    ...   0        1    ...      1   ...     1     0   ...   0
 *  (1,1,2)       0      1    ...   0        0    ...      0   ...     1     0   ...   0
 *  . . . .
 *  (9,9,8)
 *  (9,9,9)
 * 
 */
template<int sqrt_of_size>
void Sudoku<sqrt_of_size>::get_ECP_matrix(bool** matrix) {
    const int m = size*size*size;
    const int n = 4*size*size;
    
    std::div_t dv;
    enum class Conditions {row=0, column=1, square=2, cell=3} condition_type;
    for(int i = 0; i < m; i++) {
        Triple<sqrt_of_size> triple(i);
        if(!grid[triple.row-1][triple.column-1].is_blank() && !found(triple))
        {
            // incorporate specific sudoku problem data
            for(int j = 0; j < n; j++) matrix[i][j] = 0;
        } else {        // NOTE: if grid[triple.row-1][triple.column-1] == triple.value,
                        // then the solution finding process can perhaps be sped up by removing
                        // the appropriate rows and columns from the matrix and adding them
                        // to the solution
                        // this is a TODO
            for(int j = 0; j < n; j++) {
                dv = std::div(j,size*size);
                condition_type = (Conditions)dv.quot;     // between 0 and 3 inclusive
                dv = std::div(dv.rem,size);
                if(condition_type == Conditions::row) { 
                    num_t<size> row(dv.quot+1), value(dv.rem+1);
                    matrix[i][j] = triple.row == row && triple.value == value;
                } 
                else if(condition_type == Conditions::column) {
                    num_t<size> column = dv.quot+1, value = dv.rem+1;
                    matrix[i][j] = triple.column == column && triple.value == value;
                } 
                else if(condition_type == Conditions::square) {
                    num_t<size> square(dv.quot+1), value(dv.rem+1);
                    matrix[i][j] = triple.is_in_square(square) && triple.value == value;
                } 
                else if(condition_type == Conditions::cell) {
                    num_t<size> row(dv.quot+1), column(dv.rem+1);
                    matrix[i][j] = triple.row == row && triple.column == column;
                }
            }
        }
    }
    
}


template<int sqrt_of_size>
void Sudoku<sqrt_of_size>::convert_ECP_solution(std::vector<int> solution)
{
    int index;
    while(!solution.empty()) {
        index = solution.back();
        solution.pop_back();
        write( Triple<sqrt_of_size>(index) );
    }
}



int pad(int num, int width)
{
    return num;     // do general case
}

template<int sqrt_of_size>
void Sudoku<sqrt_of_size>::display_ASCII() const
{
    const int MAX_WIDTH = 1;   // do general case
    const char *blank = " ";   // in general blank = MAX_WIDTH*" "
    const char *sp = " ";
    const char * hborder = "=========================";
    const char * vborder = "|";
    const char * indent = "\t";
    
    std::cout << indent << hborder << std::endl;
    for(int i = 0; i < size; i++) {
        std::cout << indent;
        std::cout << vborder << sp;
        for(int j = 0; j < size; j++) {
            if(!grid[i][j].is_blank())
                std::cout << pad((int)grid[i][j],MAX_WIDTH) << sp;
            else
                std::cout << blank << sp;
            if( (j+1)%sqrt_of_size == 0 ) std::cout << vborder << sp;
        }
        if( (i+1)%sqrt_of_size == 0 )
            std::cout << std::endl << indent << hborder;
        std::cout <<  std::endl;
    }
}





/********************************************************************************************************
 *                                   Triple method implementations
 */

template<int sqrt_of_size> 
int Triple<sqrt_of_size>::to_index() 
{
    return (row-1)*size*size + (column-1)*size + (value-1);
}

template<int sqrt_of_size>
Triple<sqrt_of_size>::Triple(int index) {
    // index = (a-1)*size^2 + (b-1)*size + (c-1)
    // need to find a,b, and c
    std::div_t dv = std::div(index,size);
    value = dv.rem + 1;         // c = index % size + 1, etc
    dv = std::div(dv.quot,size);
    column = dv.rem + 1;
    row = dv.quot + 1;
}

template<int sqrt_of_size>
bool Triple<sqrt_of_size>::is_in_square(num_t<size> square)
{   
    int a = std::div(row-1,sqrt_of_size).quot + 1;
    int b = std::div(column-1,sqrt_of_size).quot;
    num_t<sqrt_of_size*sqrt_of_size> s = a + b*sqrt_of_size;
    return s == square;
}

/********************************************************************************************************/

} // namespace 'sudoku_GJK'



using Sudoku = sudoku_GJK::Sudoku<3>;

int sudoku_main(void) {
    // ************************************
    // ******* DANCING LINKS DEMO *********
    std::cout << "DANCING LINKS DEMO:\n\n";
    // add demo code
    
    // ************************************
    // *********** SUDOKU DEMO ************
    std::cout << "SUDOKU DEMO:\n\n";
    // We represent the sudoku grid data as a C-style string of length 9x9.  Zeroes represent blank spaces.
    // The data represents the following sudoku:
    /*
        =========================
        | 5 3   |   7   |       |
        | 6     | 1 9 5 |       |
        |   9 8 |       |   6   |
        =========================
        | 8     |   6   |     3 |
        | 4     | 8   3 |     1 |
        | 7     |   2   |     6 |
        =========================
        |   6   |       | 2 8   |
        |       | 4 1 9 |     5 |
        |       |   8   |   7 9 |
        =========================
    */
    char data[82] = 
          "530070000600195000098000060800060003400803001700020006060000280000419005000080079";
    // Create an instance of a sudoku puzzle
    Sudoku Sdku(data);
    std::cout << "The puzzle to be solved is:\n";
    // This method displays the sudoku nicely
    Sdku.display_ASCII();
    std::cout << "Solving . . .\n";
    std::clock_t start = std::clock();
    // This method solves the sudoku by converting it
    // to a matrix exact cover problem and using the
    // dancing links solution
    Sdku.solve();
    std::clock_t end = std::clock();
    std::cout << "Solution:\n";
    // Display the solved puzzle
    Sdku.display_ASCII();
    std::cout << "Elapsed time = " << 1000.0 * (end-start)/CLOCKS_PER_SEC << " ms" << " (CPU time)\n";
    
    return 0;  
}


















using linked_matrix_GJK::LMatrix;
using linked_matrix_GJK::MNode;
using linked_matrix_GJK::Column;

using dancing_links_GJK::choose_column;
using dancing_links_GJK::update;
using dancing_links_GJK::downdate;
using dancing_links_GJK::DLX;

using dancing_links_GJK::Exact_Cover_Solver;

#define NUM_TESTS 7

/****************************************************************************************************
 *                                   IMPLEMENTATION OF TESTS
 * **************************************************************************************************/

// TEST CASES
//
//
//


void matrix_creator(bool **matrix,int m, int n, bool* flat_matrix)
{   
    int j = 0;
    for(int i = 0; i < m; i++) {
        matrix[i] = new bool[n];
        for(; j < (i+1)*n; j++) {
            matrix[i][j-i*n] = flat_matrix[j];
        }
    }
}

void matrix_deletor(bool **matrix, int m, int n)
{
    for(int i = 0; i < m; i++) {
        delete[] matrix[i];
    }
    delete[] matrix;
}

void matrix_printer(bool **matrix, int m, int n)
{
    // print original matrix
    std::cout << '\t' << "BOOLEAN MATRIX" << std::endl << std::endl;
    for(int i = 0; i < m; i++) {
        std::cout << '\t';
        for(int j = 0; j < n; j++) {
            std::cout << matrix[i][j] << " ";
        }
        std::cout << '\t' << '\t' << "row " << i << std::endl;
    }
    std::cout << std::endl;
}

void print_solution(std::vector<int>& solution) {
    if(solution.empty()) {
        std::cout << "No solution exists" << std::endl;
        return;
    }
    int s = solution.size();
    std::cout << "Rows:";
    for(int i = 0; i < s; i++) {
        std::cout << '\t' << solution[i];
    }
    std::cout << std::endl;
}


void cont(void) {
    // std::cout << std::endl << "continue? [Y/n]";
    // char in;
    // std::cin >> in;
    // assert( in == 'y' || in == 'Y');
}

void test_update_downdate()
{
    int m = 4, n = 3;
    bool flat_matrix[4*3] = {
        1,0,0,
        1,1,0,
        1,0,1,
        1,0,1
    };
    bool **matrix = new bool*[m];
    matrix_creator(matrix,m,n,flat_matrix);
    matrix_printer(matrix,m,n);
    
    LMatrix M(matrix,m,n);

    cont();
    
    Column* c = static_cast<Column*>(M.head()->right());
    dancing_links_GJK::S_Stack solution;
    dancing_links_GJK::H_Stack history;
    MNode* r = c->down();
    update(M,solution,history,r);
    M.DEBUG_display();
    cont();
    downdate(M,solution,history);
    M.DEBUG_display();
    cont(); 
    r = r->down();
    update(M,solution,history,r);
    M.DEBUG_display();
    cont();
    downdate(M,solution,history);       // seg faults - fixed now
    M.DEBUG_display();
    cont();
    r = r->down();
    update(M,solution,history,r);
    M.DEBUG_display();
    cont();
    downdate(M,solution,history);
    M.DEBUG_display();
    cont();
    r = r->down();
    update(M,solution,history,r);
    M.DEBUG_display();
    cont();
    downdate(M,solution,history);
    M.DEBUG_display();
    cont();
}



void test_0()
{   
    int m = 4, n = 3;
    bool flat_matrix[4*3] = {
        1,0,0,
        1,1,0,
        0,0,1,
        0,0,1
    };
    bool **matrix = new bool*[m];
    matrix_creator(matrix,m,n,flat_matrix);
    matrix_printer(matrix,m,n);
    std::vector<int> solution = Exact_Cover_Solver(matrix,m,n);
    std::cout << std::endl << '\t' << "SOLUTION" << std::endl << std::endl;
    print_solution(solution);
    matrix_deletor(matrix,m,n);
    std::cout << std::endl << "Is output correct? [Y/n]";
    // char in;
    // std::cin >> in;
    // assert( in == 'y' || in == 'Y');
}

void test_1()
{   
    int m = 4, n = 3;
    bool flat_matrix[4*3] = {
        1,0,0,
        1,1,0,
        1,0,1,
        1,0,1
    };
    bool **matrix = new bool*[m];
    matrix_creator(matrix,m,n,flat_matrix);
    matrix_printer(matrix,m,n);
    std::vector<int> solution = Exact_Cover_Solver(matrix,m,n);
    std::cout << std::endl << '\t' << "SOLUTION" << std::endl << std::endl;
    print_solution(solution);
    matrix_deletor(matrix,m,n);
    std::cout << std::endl << "Is output correct? [Y/n]";
    // char in;
    // std::cin >> in;
    // assert( in == 'y' || in == 'Y');
}

bool continue_prompt(void) {
    std::cout << std::endl << "continue? [Y/n]";
    // char in;
    // std::cin >> in;
    // return ( in == 'y' || in == 'Y');
    return true;
}

void test_2()
{   
do {
    int m = 8, n = 7;
    bool **matrix = new bool*[m];
    srand((int) time(0));
    bool flat_matrix[8*7];
    for(int k = 0; k < m*n; k++) flat_matrix[k] = (bool) (rand() % 2);

    matrix_creator(matrix,m,n,flat_matrix);
    matrix_printer(matrix,m,n);
    std::vector<int> solution = Exact_Cover_Solver(matrix,m,n);
    std::cout << std::endl << '\t' << "SOLUTION" << std::endl << std::endl;
    print_solution(solution);
    matrix_deletor(matrix,m,n);
    std::cout << std::endl << "Is output correct? [Y/n]";
    // char in;
    // std::cin >> in;
    // assert( in == 'y' || in == 'Y');
} while(continue_prompt());
}


void test_big_random_matrix()
{
    do {
    int m = 729, n = 4*81;
    bool **matrix = new bool*[m];
    srand((int) time(0));
    bool flat_matrix[m*n];
    for(int k = 0; k < m*n; k++) flat_matrix[k] = (bool) (rand() % 4 != 0);

    matrix_creator(matrix,m,n,flat_matrix);
    //matrix_printer(matrix,m,n);
    std::vector<int> solution = Exact_Cover_Solver(matrix,m,n);
    std::cout << std::endl << '\t' << "SOLUTION" << std::endl << std::endl;
    print_solution(solution);
    matrix_deletor(matrix,m,n);
} while(continue_prompt());
}


void test_big_soluble_matrix()
{
    do {
    int m = 729, n = 4*81;
    bool **matrix = new bool*[m];
    srand((int) time(0));
    bool flat_matrix[m*n];
    int k = 0;
    for(; k < 4*81*400; k++) flat_matrix[k] = (bool) (rand() % 4 != 0);
    for(; k < 4*81*400 + 4*81; k++) flat_matrix[k] = 1;        // this row is the (a) solution
    for(; k < m*n; k++) flat_matrix[k] = (bool) (rand() % 4 != 0);

    matrix_creator(matrix,m,n,flat_matrix);
    //matrix_printer(matrix,m,n);
    std::vector<int> solution = Exact_Cover_Solver(matrix,m,n);
    std::cout << std::endl << '\t' << "SOLUTION" << std::endl << std::endl;
    print_solution(solution);
    matrix_deletor(matrix,m,n);
    } while(continue_prompt());
}

void test_sparse_matrix()
{
    do {
        int m = 729, n = 4*81;
        bool **matrix = new bool*[m];
        srand((int) time(0));
        for(int i = 0; i < m; i++) {
            matrix[i] = new bool[n];
            for(int j = 0; j < n; j++) {
            matrix[i][j] = 0;
            }
            matrix[i][(rand() % 81)] = 1;
            matrix[i][81 + (rand() % 81)] = 1;
            matrix[i][2*81 + (rand() % 81)] = 1;
            matrix[i][3*81 + (rand() % 81)] = 1;
        }

    //matrix_printer(matrix,m,n);
    LMatrix M(matrix,m,n);
//    std::ofstream ofs(".\\lmatrix.txt",std::ofstream::app);
//    M.DEBUG_display(ofs);
//    ofs.close();
    std::vector<int> solution = Exact_Cover_Solver(matrix,m,n);
    std::cout << std::endl << '\t' << "SOLUTION" << std::endl << std::endl;
    print_solution(solution);
    matrix_deletor(matrix,m,n);
} while(continue_prompt());
}


/****************************************************************************************************
 *                             END OF IMPLEMENTATION OF TESTS
 * **************************************************************************************************/


typedef void (*PROC)(void);
const PROC tests[NUM_TESTS] = {
&test_update_downdate,
&test_0,
&test_1,
&test_2,
&test_big_random_matrix,
&test_big_soluble_matrix,
&test_sparse_matrix
};

// run all tests
int main() {
    bool TESTS[NUM_TESTS];  // will determine whether a given test is to be run or omitted
    for(int i = 0; i < NUM_TESTS; i++) TESTS[i] = 1;

	for(int i = 0; i < NUM_TESTS; i++) {
		if(TESTS[i]) {
			tests[i]();
            std::cout << "Test " << i << " passed!" << std::endl;
		}
	}
	return 0;
}
