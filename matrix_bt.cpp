#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <omp.h> // Librería para paralelismo

#define INF 9999
#define N 14 // Usa el valor donde tu PC tardó 46s para notar la mejora

int globalMinDist = INF;

void backtracking(int matrix[N][N], int visited[N], int current, int end, int dist, int *localMin) {
    if (current == end) {
        if (dist < *localMin) {
            *localMin = dist;
        }
        return;
    }

    visited[current] = 1;

    for (int i = 0; i < N; i++) {
        if (matrix[current][i] < INF && !visited[i]) {
            backtracking(matrix, visited, i, end, dist + matrix[current][i], localMin);
        }
    }
    visited[current] = 0;
}

int main(void) {
    int matrix[N][N];
    int start = 0;
    int end = N - 1;

    srand(12345);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (i == j) matrix[i][j] = INF;
            else if (i < j) matrix[i][j] = rand() % 50 + 10;
            else matrix[i][j] = matrix[j][i];
        }
    }

    // --- EJECUCIÓN PARALELA ---
    double start_t = omp_get_wtime(); 

    #pragma omp parallel
    {
        int localMin = INF; // Cada núcleo tiene su propio mínimo local
        int visited[N] = {0};
        
        // Repartimos las primeras decisiones del camino entre los núcleos
        #pragma omp for schedule(dynamic)
        for (int i = 0; i < N; i++) {
            if (matrix[start][i] < INF) {
                visited[start] = 1;
                backtracking(matrix, visited, i, end, matrix[start][i], &localMin);
            }
        }

        // Actualización segura del mínimo global
        #pragma omp critical
        {
            if (localMin < globalMinDist) globalMinDist = localMin;
        }
    }

    double end_t = omp_get_wtime();
    // ---------------------------

    printf("N=%d | Hilos: %d | Tiempo: %.2f ms | MinDist: %d\n", 
            N, omp_get_max_threads(), (end_t - start_t) * 1000, globalMinDist);
    return 0;
}
