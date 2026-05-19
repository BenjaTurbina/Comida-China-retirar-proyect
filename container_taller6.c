#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define MAX_ITEMS 64
#define INITIAL_DEPTH 3

typedef struct {
    char id[5];
    int id_original;
    int hora; // Guardado en minutos desde las 00:00 para facilitar filtros
    double weight;
    double volume;
    double original_value;
    double effective_value;
    double relative_use;
    double priority_ratio;
} Package;

typedef struct {
    int level;
    double weight;
    double volume;
    double value;
    double bound;
    int taken[MAX_ITEMS];
} Node;

typedef struct {
    Node nodes[1024];
    int count;
} Frontier;

// Base de datos global con la serie de tiempo completa (Tabla 1)
Package dataset[10] = {
    {"P1",  0,  8*60+0,  120, 8,  80},
    {"P2",  1,  8*60+10, 300, 20, 120},
    {"P3",  2,  8*60+20, 250, 12, 150},
    {"P4",  3,  8*60+30, 100, 5,  60},
    {"P5",  4,  8*60+40, 180, 15, 100},
    {"P6",  5,  9*60+0,  400, 25, 170},
    {"P7",  6,  9*60+15, 90,  3,  55},
    {"P8",  7,  9*60+30, 220, 10, 130},
    {"P9",  8,  9*60+45, 150, 7,  90},
    {"P10", 9, 10*60+0,  350, 18, 160}
};

// Variables de ejecución para la ventana y contenedor actual
int N;
double MAX_WEIGHT;
double MAX_VOLUME;
Package packages[MAX_ITEMS];

double best_value = 0.0;
int best_taken[MAX_ITEMS] = {0};

int cmp_packages(const void *a, const void *b) {
    const Package *x = (const Package *)a;
    const Package *y = (const Package *)b;
    if (y->priority_ratio > x->priority_ratio) return 1;
    if (y->priority_ratio < x->priority_ratio) return -1;
    return 0;
}

double compute_bound(Node *u) {
    if (u->weight >= MAX_WEIGHT || u->volume >= MAX_VOLUME) return 0.0;

    double bound = u->value;
    double total_weight = u->weight;
    double total_volume = u->volume;
    int j = u->level;

    while (j < N && total_weight + packages[j].weight <= MAX_WEIGHT && total_volume + packages[j].volume <= MAX_VOLUME) {
        total_weight += packages[j].weight;
        total_volume += packages[j].volume;
        bound += packages[j].effective_value;
        j++;
    }

    if (j < N) {
        double rem_w = MAX_WEIGHT - total_weight;
        double rem_v = MAX_VOLUME - total_volume;
        double frac_w = rem_w / packages[j].weight;
        double frac_v = rem_v / packages[j].volume;
        double fraction = (frac_w < frac_v) ? frac_w : frac_v;

        if (fraction > 0.0) {
            bound += packages[j].effective_value * fraction;
        }
    }
    return bound;
}

void try_update_best(Node *u) {
    if (u->value > best_value) {
        #pragma omp critical
        {
            if (u->value > best_value) {
                best_value = u->value;
                memcpy(best_taken, u->taken, sizeof(int) * N);
            }
        }
    }
}

void dfs_branch_bound(Node start) {
    Node stack[2048];
    int top = 0;
    stack[top++] = start;

    while (top > 0) {
        Node u = stack[--top];

        if (u.bound <= best_value) continue;
        if (u.level >= N) {
            try_update_best(&u);
            continue;
        }

        int idx = u.level;

        // Rama 1: Incluir paquete
        Node with = u;
        with.level = idx + 1;
        with.weight += packages[idx].weight;
        with.volume += packages[idx].volume;
        with.value += packages[idx].effective_value;
        with.taken[idx] = 1;

        if (with.weight <= MAX_WEIGHT && with.volume <= MAX_VOLUME) {
            try_update_best(&with);
            with.bound = compute_bound(&with);
            if (with.bound > best_value) stack[top++] = with;
        }

        // Rama 2: Excluir paquete
        Node without = u;
        without.level = idx + 1;
        without.taken[idx] = 0;
        without.bound = compute_bound(&without);
        if (without.bound > best_value) stack[top++] = without;
    }
}

void generate_frontier(Frontier *frontier) {
    frontier->count = 0;
    Node root;
    root.level = 0;
    root.weight = 0.0;
    root.volume = 0.0;
    root.value = 0.0;
    memset(root.taken, 0, sizeof(root.taken));
    root.bound = compute_bound(&root);

    Node queue[1024];
    int qh = 0, qt = 0;
    queue[qt++] = root;

    while (qh < qt) {
        Node u = queue[qh++];
        if (u.level >= INITIAL_DEPTH || u.level >= N) {
            frontier->nodes[frontier->count++] = u;
            continue;
        }

        int idx = u.level;
        Node with = u;
        with.level = idx + 1;
        with.weight += packages[idx].weight;
        with.volume += packages[idx].volume;
        with.value += packages[idx].effective_value;
        with.taken[idx] = 1;

        if (with.weight <= MAX_WEIGHT && with.volume <= MAX_VOLUME) {
            with.bound = compute_bound(&with);
            if (with.bound > best_value) queue[qt++] = with;
        }

        Node without = u;
        without.level = idx + 1;
        without.taken[idx] = 0;
        without.bound = compute_bound(&without);
        if (without.bound > best_value) queue[qt++] = without;
    }
}

void filtrar_ventana(int minutos_inicio, int minutos_fin) {
    N = 0;
    for (int i = 0; i < 10; i++) {
        if (dataset[i].hora >= minutos_inicio && dataset[i].hora <= minutos_fin) {
            packages[N] = dataset[i];
            N++;
        }
    }
}

void evaluar_configuracion(double max_w, double max_v) {
    MAX_WEIGHT = max_w;
    MAX_VOLUME = max_v;
    best_value = 0.0;
    memset(best_taken, 0, sizeof(best_taken));

    // REQUERIMIENTO 4 & 5: Valores logísticos dinámicos y ratios
    for (int i = 0; i < N; i++) {
        double uso_vol = packages[i].volume / MAX_VOLUME;
        if (uso_vol < 0.10) {
            packages[i].effective_value = packages[i].original_value;
        } else if (uso_vol <= 0.20) {
            packages[i].effective_value = packages[i].original_value * 0.95;
        } else {
            packages[i].effective_value = packages[i].original_value * 0.90;
        }

        packages[i].relative_use = (packages[i].weight / MAX_WEIGHT) + (packages[i].volume / MAX_VOLUME);
        packages[i].priority_ratio = packages[i].effective_value / packages[i].relative_use;
    }

    // REQUERIMIENTO 6: Ordenar por prioridad
    qsort(packages, N, sizeof(Package), cmp_packages);

    Frontier frontier;
    generate_frontier(&frontier);

    printf("Container evaluado: %.0f m3\n", max_v);

    // REQUERIMIENTO 8: Exploración en paralelo con OpenMP
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < frontier.count; i++) {
        // REQUERIMIENTO 9: Mostrar qué thread explora cada subárbol
        int tid = omp_get_thread_num();
        #pragma omp critical
        {
            printf("  Thread %d explora subarbol %d\n", tid, i);
        }

        dfs_branch_bound(frontier.nodes[i]);
    }

    // REQUERIMIENTO 10: Reportar la mejor asignación encontrada
    printf(" Mejor valor efectivo: %.2f\n", best_value);
    
    double peso_total = 0, vol_total = 0;
    char seleccionados[256] = "";
    for (int i = 0; i < N; i++) {
        if (best_taken[i]) {
            peso_total += packages[i].weight;
            vol_total += packages[i].volume;
            strcat(seleccionados, packages[i].id);
            strcat(seleccionados, ", ");
        }
    }
    if (strlen(seleccionados) > 0) {
        seleccionados[strlen(seleccionados) - 2] = '\0'; // Quita la última coma
    }

    printf(" Peso usado: %.0f / %.0f kg\n", peso_total, MAX_WEIGHT);
    printf(" Volumen usado: %.0f / %.0f m3\n", vol_total, MAX_VOLUME);
    printf(" Paquetes seleccionados: %s\n\n", seleccionados);
}

int main() {
    omp_set_nested(1); // Permite anidamiento si fuera necesario

    // VENTANA 1: 08:00 - 08:59
    printf("Ventana temporal: 08:00 08:59\n");
    printf("=============================\n");
    filtrar_ventana(8*60+0, 8*60+59);
    evaluar_configuracion(600, 30);
    evaluar_configuracion(850, 45);
    evaluar_configuracion(1000, 60);

    // VENTANA 2: 09:00 - 09:59
    printf("Ventana temporal: 09:00 09:59\n");
    printf("=============================\n");
    filtrar_ventana(9*60+0, 9*60+59);
    evaluar_configuracion(600, 30);
    evaluar_configuracion(850, 45);
    evaluar_configuracion(1000, 60);

    // VENTANA 3: 10:00 - 10:59
    printf("Ventana temporal: 10:00 10:59\n");
    printf("=============================\n");
    filtrar_ventana(10*60+0, 10*60+59);
    evaluar_configuracion(600, 30);
    evaluar_configuracion(850, 45);
    evaluar_configuracion(1000, 60);

    return 0;
}
