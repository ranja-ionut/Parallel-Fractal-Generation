/*
 * APD - Tema 1
 * Octombrie 2020
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })


char *in_filename_julia;
char *in_filename_mandelbrot;
char *out_filename_julia;
char *out_filename_mandelbrot;

// structura pentru un numar complex
typedef struct _complex {
	double a;
	double b;
} complex;

// structura pentru parametrii unei rulari
typedef struct _params {
	int is_julia; // parametru independent
	complex c_julia; // parametru independent

	/**
	 * Parametrii comuni atat pentru julia cat si pentru mandelbrot
	 * au fost pusi intr-o structura anonima, fiind accesati drept
	 * campuri a instantei acestei structuri, permitand separarea
	 * acestora fara a fi necesara crearea unor variabile globale
	 * sau poluarea structurii cu cate 2 campuri de acelasi tip,
	 * unul pentru julia, iar celalalt pentru mandelbrot
	 */
	struct {
		int iterations, width, height;
		int **result;
		double x_min, x_max, y_min, y_max, resolution;
	} julia, mandelbrot;
} params;

// numarul de thread-uri
int P;

// bariera
pthread_barrier_t barrier;

/**
 * Structura folosita pentru a oferi fiecarui thread un id si pentru
 * a putea lucra cu o singura variabila de tip params, fara a fi nevoie de
 * variabile globale. Campul par va fi un pointer la variabila din main.
 */
typedef struct _args {
	int id;
	params *par;
}args;

// citeste argumentele programului
void get_args(int argc, char **argv)
{
	if (argc < 6) {
		printf("Numar insuficient de parametri:\n\t"
				"./tema1 fisier_intrare_julia fisier_iesire_julia "
				"fisier_intrare_mandelbrot fisier_iesire_mandelbrot P\n");
		exit(1);
	}

	in_filename_julia = argv[1];
	out_filename_julia = argv[2];
	in_filename_mandelbrot = argv[3];
	out_filename_mandelbrot = argv[4];
	P = atoi(argv[5]);

	if (P == 0) {
		printf("Argumentul P este invalid! Trebuie sa fie un numar >= 1!\n");
		exit(1);
	}

	pthread_barrier_init(&barrier, NULL, P);
}

// citeste fisierul de intrare
void read_input_file(char *in_filename, params* par)
{
	FILE *file = fopen(in_filename, "r");
	if (file == NULL) {
		printf("Eroare la deschiderea fisierului de intrare!\n");
		exit(1);
	}

	fscanf(file, "%d", &par->is_julia);

	if (par->is_julia) {
		fscanf(file, "%lf %lf %lf %lf",
			&par->julia.x_min, &par->julia.x_max, &par->julia.y_min,
			&par->julia.y_max);
		fscanf(file, "%lf", &par->julia.resolution);
		fscanf(file, "%d", &par->julia.iterations);
	} else {
		fscanf(file, "%lf %lf %lf %lf",
			&par->mandelbrot.x_min, &par->mandelbrot.x_max, 
			&par->mandelbrot.y_min, &par->mandelbrot.y_max);
		fscanf(file, "%lf", &par->mandelbrot.resolution);
		fscanf(file, "%d", &par->mandelbrot.iterations);
	}

	if (par->is_julia) {
		fscanf(file, "%lf %lf", &par->c_julia.a, &par->c_julia.b);
	}

	fclose(file);
}

// scrie rezultatul in fisierul de iesire
void write_output_file(char *out_filename, int **result, int width, int height)
{
	int i, j;

	FILE *file = fopen(out_filename, "w");
	if (file == NULL) {
		printf("Eroare la deschiderea fisierului de iesire!\n");
		return;
	}

	fprintf(file, "P2\n%d %d\n255\n", width, height);
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			fprintf(file, "%d ", result[i][j]);
		}
		fprintf(file, "\n");
	}

	fclose(file);
}

// ruleaza tema
void *run_thread_function(void *arg) {
	args *ar = (args*)arg;
	int id = ar->id; // se ia thread id
	params *par = ar->par; // se obtine pointer la variabila din main

	int w, h, start, end;

	start = id * (double)par->julia.height / P;
	end = min((id + 1) * (double)par->julia.height / P, par->julia.height);

	/**
	 * Se vor aloca liniile din matricea result specifice fiecarui thread.
	 * De exemplu, pentru thread-ul i vom avea intervalul [starti; endi]
	 * pentru care se vor realiza calculele, dar pentru a scrie rezultatul
	 * sunt necesare liniile din intervalul 
	 *				 [height - endi; height - starti - 1]
	 * pentru a putea scrie direct in coordonate ecran, fara a fi necesara
	 * conversia de coordonate.
	 */
	for (h = par->julia.height - end; h <= par->julia.height - start - 1; h++) {
		par->julia.result[h] = malloc(par->julia.width * sizeof(int));
		if (par->julia.result[h] == NULL) {
			printf("Eroare la malloc julia 2!\n");
			exit(1);
		}
	}

	// Se ruleaza algoritmul julia paralelizat
	for (w = 0; w < par->julia.width; w++) {
		for (h = start; h < end; h++) {
			int step = 0;
			complex z = { .a = w * par->julia.resolution
									+ par->julia.x_min,
							.b = h * par->julia.resolution
									+ par->julia.y_min };

			while (sqrt(pow(z.a, 2.0) + pow(z.b, 2.0)) < 2.0 
					&& step < par->julia.iterations) {
				complex z_aux = { .a = z.a, .b = z.b };

				z.a = pow(z_aux.a, 2) - pow(z_aux.b, 2) + par->c_julia.a;
				z.b = 2 * z_aux.a * z_aux.b + par->c_julia.b;

				step++;
			}

			// Se scrie rezultatul direct in coordonate ecran
			par->julia.result[par->julia.height - h - 1][w] = step % 256;
		}
	}

	/**
	 * Bariera care asigura ca toate thread-urile au terminat calculele
	 * si se poate efectua scrierea in fisierul de iesire.
	 */
	pthread_barrier_wait(&barrier);

	if (id == 0) {
		write_output_file(out_filename_julia, par->julia.result,
							par->julia.width, par->julia.height);
	}

	/**
	 * Bariera care asigura ca scrierea in fisierul de iesire a fost realizata
	 * cu succes si se poate elibera memoria ocupata de fiecare thread.
	 */
	pthread_barrier_wait(&barrier);

	for (h = start; h < end; h++) {
		free(par->julia.result[h]);
	}

	start = id * (double)par->mandelbrot.height / P;
	end = min((id + 1) * (double)par->mandelbrot.height / P,
			par->mandelbrot.height);

	// Acelasi rationament ca mai sus in cazul multimii julia
	for (h = par->mandelbrot.height - end;
		 h <= par->mandelbrot.height - start - 1; h++) {
		par->mandelbrot.result[h] = malloc(par->mandelbrot.width * sizeof(int));
		if (par->mandelbrot.result[h] == NULL) {
			printf("Eroare la malloc julia 2!\n");
			exit(1);
		}
	}

	// Se ruleaza algoritmul mandelbrot paralelizat
	for (w = 0; w < par->mandelbrot.width; w++) {
		for (h = start; h < end; h++) {
			complex c = { .a = w * par->mandelbrot.resolution
								+ par->mandelbrot.x_min,
							.b = h * par->mandelbrot.resolution
								+ par->mandelbrot.y_min };
			complex z = { .a = 0, .b = 0 };
			int step = 0;

			while (sqrt(pow(z.a, 2.0) + pow(z.b, 2.0)) < 2.0 
					&& step < par->mandelbrot.iterations) {
				complex z_aux = { .a = z.a, .b = z.b };

				z.a = pow(z_aux.a, 2.0) - pow(z_aux.b, 2.0) + c.a;
				z.b = 2.0 * z_aux.a * z_aux.b + c.b;

				step++;
			}

			par->mandelbrot.result[par->mandelbrot.height - h - 1][w]
														= step % 256;
		}
	}

	/**
	 * Bariera care asigura ca toate thread-urile au terminat calculele
	 * si se poate efectua scrierea in fisierul de iesire.
	 */
	pthread_barrier_wait(&barrier);

	if (id == 0) {
		write_output_file(out_filename_mandelbrot, par->mandelbrot.result,
							par->mandelbrot.width, par->mandelbrot.height);
	}

	/**
	 * Bariera care asigura ca scrierea in fisierul de iesire a fost realizata
	 * cu succes si se poate elibera memoria ocupata de fiecare thread.
	 */
	pthread_barrier_wait(&barrier);
	
	for (h = start; h < end; h++) {
		free(par->mandelbrot.result[h]);
	}

	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	params par;

	// se citesc argumentele programului
	get_args(argc, argv);

	// Julia:
	// - se citesc parametrii de intrare
	// - se aloca tabloul cu rezultatul
	read_input_file(in_filename_julia, &par);
	par.julia.width = (par.julia.x_max - par.julia.x_min)
						/ par.julia.resolution;
	par.julia.height = (par.julia.y_max - par.julia.y_min)
						/ par.julia.resolution;
	par.julia.result = malloc(par.julia.height * sizeof(int*));
	if (par.julia.result == NULL) {
		printf("Eroare la malloc julia!\n");
		exit(1);
	}

	// Mandelbrot:
	// - se citesc parametrii de intrare
	// - se aloca tabloul cu rezultatul
	read_input_file(in_filename_mandelbrot, &par);
	par.mandelbrot.width = (par.mandelbrot.x_max - par.mandelbrot.x_min)
							/ par.mandelbrot.resolution;
	par.mandelbrot.height = (par.mandelbrot.y_max - par.mandelbrot.y_min)
							/ par.mandelbrot.resolution;
	par.mandelbrot.result = malloc(par.mandelbrot.height * sizeof(int*));
	if (par.mandelbrot.result == NULL) {
		printf("Eroare la malloc mandelbrot!\n");
		exit(1);
	}

	// se creeaza thread-urile
	pthread_t threads[P];
  	int r, id;
  	void *status;
	args args[P];

  	for (id = 0; id < P; id++) {
		args[id].id = id;
		args[id].par = &par; // pointer la aceeasi variabila par (la adresa ei)
		r = pthread_create(&threads[id], NULL, 
							run_thread_function, &(args[id]));

		if (r) {
	  		printf("Eroare la crearea thread-ului %i\n", id);
	  		exit(-1);
		}
  	}

	// se asteapta join-ul thread-urilor
	for (id = 0; id < P; id++) {
		r = pthread_join(threads[id], &status);

		if (r) {
	  		printf("Eroare la asteptarea thread-ului %i\n", id);
	  		exit(-1);
		}
  	}

	// eliberarea memoriei
	pthread_barrier_destroy(&barrier);
	free(par.julia.result);
	free(par.mandelbrot.result);

	return 0;
}
