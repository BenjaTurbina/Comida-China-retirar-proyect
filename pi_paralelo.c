#include <stdio.h>
#include <omp.h>
#include <time.h>

int main(){
    clock_t inicio,fin;
    double time_ms;
    inicio = clock();
    float pi = 0;
    int n = 1000;
    #pragma omp parallel for 
    for (int i = 1; i <= n; i++) {
    if (i % 2 != 0) {
        pi += 4.0 / (2 * i - 1);
        } 
    else {
        pi -= 4.0 / (2 * i - 1);
        }
    }
    fin = clock();
    time_ms = ((double)(fin-inicio)/CLOCKS_PER_SEC)*1000;
    printf("Valor de pi: %.20f\n",pi);
    printf("Tiempo : %.2f milisegundos", time_ms);
    return 0;
}