#include <bits/stdc++.h>
#include "dancing_links.hpp"

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

// namespace dancing_links_GJK {
//     std::vector<int> Exact_Cover_Solver(linked_matrix_GJK::LMatrix&);
// }

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
    dancing_links::LMatrix M(matrix,m,n);
    // free memory
    for(int i=0; i < m; i++) {
        delete[] matrix[i];
    }
    delete[] matrix;

    std::vector<int> solution = dancing_links::solve_exact_cover(M);
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

int main(void) {
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


















// using linked_matrix_GJK::LMatrix;
// using linked_matrix_GJK::MNode;
// using linked_matrix_GJK::Column;

// using dancing_links_GJK::choose_column;
// using dancing_links_GJK::update;
// using dancing_links_GJK::downdate;
// using dancing_links_GJK::DLX;

// using dancing_links_GJK::Exact_Cover_Solver;

// #define NUM_TESTS 7

// /****************************************************************************************************
//  *                                   IMPLEMENTATION OF TESTS
//  * **************************************************************************************************/

// // TEST CASES
// //
// //
// //


// void matrix_creator(bool **matrix,int m, int n, bool* flat_matrix)
// {   
//     int j = 0;
//     for(int i = 0; i < m; i++) {
//         matrix[i] = new bool[n];
//         for(; j < (i+1)*n; j++) {
//             matrix[i][j-i*n] = flat_matrix[j];
//         }
//     }
// }

// void matrix_deletor(bool **matrix, int m, int n)
// {
//     for(int i = 0; i < m; i++) {
//         delete[] matrix[i];
//     }
//     delete[] matrix;
// }

// void matrix_printer(bool **matrix, int m, int n)
// {
//     // print original matrix
//     std::cout << '\t' << "BOOLEAN MATRIX" << std::endl << std::endl;
//     for(int i = 0; i < m; i++) {
//         std::cout << '\t';
//         for(int j = 0; j < n; j++) {
//             std::cout << matrix[i][j] << " ";
//         }
//         std::cout << '\t' << '\t' << "row " << i << std::endl;
//     }
//     std::cout << std::endl;
// }

// void print_solution(std::vector<int>& solution) {
//     if(solution.empty()) {
//         std::cout << "No solution exists" << std::endl;
//         return;
//     }
//     int s = solution.size();
//     std::cout << "Rows:";
//     for(int i = 0; i < s; i++) {
//         std::cout << '\t' << solution[i];
//     }
//     std::cout << std::endl;
// }


// void cont(void) {
//     // std::cout << std::endl << "continue? [Y/n]";
//     // char in;
//     // std::cin >> in;
//     // assert( in == 'y' || in == 'Y');
// }

// void test_update_downdate()
// {
//     int m = 4, n = 3;
//     bool flat_matrix[4*3] = {
//         1,0,0,
//         1,1,0,
//         1,0,1,
//         1,0,1
//     };
//     bool **matrix = new bool*[m];
//     matrix_creator(matrix,m,n,flat_matrix);
//     matrix_printer(matrix,m,n);
    
//     LMatrix M(matrix,m,n);

//     cont();
    
//     Column* c = static_cast<Column*>(M.head()->right());
//     dancing_links_GJK::S_Stack solution;
//     dancing_links_GJK::H_Stack history;
//     MNode* r = c->down();
//     update(M,solution,history,r);
//     M.DEBUG_display();
//     cont();
//     downdate(M,solution,history);
//     M.DEBUG_display();
//     cont(); 
//     r = r->down();
//     update(M,solution,history,r);
//     M.DEBUG_display();
//     cont();
//     downdate(M,solution,history);       // seg faults - fixed now
//     M.DEBUG_display();
//     cont();
//     r = r->down();
//     update(M,solution,history,r);
//     M.DEBUG_display();
//     cont();
//     downdate(M,solution,history);
//     M.DEBUG_display();
//     cont();
//     r = r->down();
//     update(M,solution,history,r);
//     M.DEBUG_display();
//     cont();
//     downdate(M,solution,history);
//     M.DEBUG_display();
//     cont();
// }



// void test_0()
// {   
//     int m = 4, n = 3;
//     bool flat_matrix[4*3] = {
//         1,0,0,
//         1,1,0,
//         0,0,1,
//         0,0,1
//     };
//     bool **matrix = new bool*[m];
//     matrix_creator(matrix,m,n,flat_matrix);
//     matrix_printer(matrix,m,n);
//     std::vector<int> solution = Exact_Cover_Solver(matrix,m,n);
//     std::cout << std::endl << '\t' << "SOLUTION" << std::endl << std::endl;
//     print_solution(solution);
//     matrix_deletor(matrix,m,n);
//     std::cout << std::endl << "Is output correct? [Y/n]";
//     // char in;
//     // std::cin >> in;
//     // assert( in == 'y' || in == 'Y');
// }

// void test_1()
// {   
//     int m = 4, n = 3;
//     bool flat_matrix[4*3] = {
//         1,0,0,
//         1,1,0,
//         1,0,1,
//         1,0,1
//     };
//     bool **matrix = new bool*[m];
//     matrix_creator(matrix,m,n,flat_matrix);
//     matrix_printer(matrix,m,n);
//     std::vector<int> solution = Exact_Cover_Solver(matrix,m,n);
//     std::cout << std::endl << '\t' << "SOLUTION" << std::endl << std::endl;
//     print_solution(solution);
//     matrix_deletor(matrix,m,n);
//     std::cout << std::endl << "Is output correct? [Y/n]";
//     // char in;
//     // std::cin >> in;
//     // assert( in == 'y' || in == 'Y');
// }

// bool continue_prompt(void) {
//     std::cout << std::endl << "continue? [Y/n]";
//     // char in;
//     // std::cin >> in;
//     // return ( in == 'y' || in == 'Y');
//     return true;
// }

// void test_2()
// {   
// do {
//     int m = 8, n = 7;
//     bool **matrix = new bool*[m];
//     srand((int) time(0));
//     bool flat_matrix[8*7];
//     for(int k = 0; k < m*n; k++) flat_matrix[k] = (bool) (rand() % 2);

//     matrix_creator(matrix,m,n,flat_matrix);
//     matrix_printer(matrix,m,n);
//     std::vector<int> solution = Exact_Cover_Solver(matrix,m,n);
//     std::cout << std::endl << '\t' << "SOLUTION" << std::endl << std::endl;
//     print_solution(solution);
//     matrix_deletor(matrix,m,n);
//     std::cout << std::endl << "Is output correct? [Y/n]";
//     // char in;
//     // std::cin >> in;
//     // assert( in == 'y' || in == 'Y');
// } while(continue_prompt());
// }


// void test_big_random_matrix()
// {
//     do {
//     int m = 729, n = 4*81;
//     bool **matrix = new bool*[m];
//     srand((int) time(0));
//     bool flat_matrix[m*n];
//     for(int k = 0; k < m*n; k++) flat_matrix[k] = (bool) (rand() % 4 != 0);

//     matrix_creator(matrix,m,n,flat_matrix);
//     //matrix_printer(matrix,m,n);
//     std::vector<int> solution = Exact_Cover_Solver(matrix,m,n);
//     std::cout << std::endl << '\t' << "SOLUTION" << std::endl << std::endl;
//     print_solution(solution);
//     matrix_deletor(matrix,m,n);
// } while(continue_prompt());
// }


// void test_big_soluble_matrix()
// {
//     do {
//     int m = 729, n = 4*81;
//     bool **matrix = new bool*[m];
//     srand((int) time(0));
//     bool flat_matrix[m*n];
//     int k = 0;
//     for(; k < 4*81*400; k++) flat_matrix[k] = (bool) (rand() % 4 != 0);
//     for(; k < 4*81*400 + 4*81; k++) flat_matrix[k] = 1;        // this row is the (a) solution
//     for(; k < m*n; k++) flat_matrix[k] = (bool) (rand() % 4 != 0);

//     matrix_creator(matrix,m,n,flat_matrix);
//     //matrix_printer(matrix,m,n);
//     std::vector<int> solution = Exact_Cover_Solver(matrix,m,n);
//     std::cout << std::endl << '\t' << "SOLUTION" << std::endl << std::endl;
//     print_solution(solution);
//     matrix_deletor(matrix,m,n);
//     } while(continue_prompt());
// }

// void test_sparse_matrix()
// {
//     do {
//         int m = 729, n = 4*81;
//         bool **matrix = new bool*[m];
//         srand((int) time(0));
//         for(int i = 0; i < m; i++) {
//             matrix[i] = new bool[n];
//             for(int j = 0; j < n; j++) {
//             matrix[i][j] = 0;
//             }
//             matrix[i][(rand() % 81)] = 1;
//             matrix[i][81 + (rand() % 81)] = 1;
//             matrix[i][2*81 + (rand() % 81)] = 1;
//             matrix[i][3*81 + (rand() % 81)] = 1;
//         }

//     //matrix_printer(matrix,m,n);
//     LMatrix M(matrix,m,n);
// //    std::ofstream ofs(".\\lmatrix.txt",std::ofstream::app);
// //    M.DEBUG_display(ofs);
// //    ofs.close();
//     std::vector<int> solution = Exact_Cover_Solver(matrix,m,n);
//     std::cout << std::endl << '\t' << "SOLUTION" << std::endl << std::endl;
//     print_solution(solution);
//     matrix_deletor(matrix,m,n);
// } while(continue_prompt());
// }


// /****************************************************************************************************
//  *                             END OF IMPLEMENTATION OF TESTS
//  * **************************************************************************************************/


// typedef void (*PROC)(void);
// const PROC tests[NUM_TESTS] = {
// &test_update_downdate,
// &test_0,
// &test_1,
// &test_2,
// &test_big_random_matrix,
// &test_big_soluble_matrix,
// &test_sparse_matrix
// };

// // run all tests
// int main() {
//     bool TESTS[NUM_TESTS];  // will determine whether a given test is to be run or omitted
//     for(int i = 0; i < NUM_TESTS; i++) TESTS[i] = 1;

// 	for(int i = 0; i < NUM_TESTS; i++) {
// 		if(TESTS[i]) {
// 			tests[i]();
//             std::cout << "Test " << i << " passed!" << std::endl;
// 		}
// 	}
// 	return 0;
// }
