#include "tiny_gp.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

const size_t POP_SIZE          = 10000; // Number of programs in a population
const size_t PROG_SIZE         = 100;   // Maximum length of a program
const double CMD_MUTATION_PROB = 0.05;  // Probability of mutation per node
const double CROSS_VS_MUT_PROB =
    0.9; // Probability of crossover between two parents
const size_t TOURNAMENT_SIZE = 2; // Tournament size for parent selection

int main(int argc, char *argv[]) {
	const char *path = NULL;
	long        seed = time(NULL);

	if (argc == 3) {
		seed = atol(argv[1]);
		path = argv[2];
	} else if (argc == 2) {
		path = argv[1];
	} else {
		printf("Usage: %s [seed?] <path/to/problem.dat>\n", argv[0]);
		return 1;
	}

	tgp_state *tgp = tgp_init(
	    path,
	    POP_SIZE,
	    PROG_SIZE,
	    CMD_MUTATION_PROB,
	    CROSS_VS_MUT_PROB,
	    TOURNAMENT_SIZE,
	    seed
	);
	if (!tgp) {
		printf("Failed to load the problem\n");
		return 1;
	}

	for (int i = 0; i < 100; i++) {
		tgp_evolve(tgp);

		// Compute avg fitness
		double avg_fit = 0;
		size_t avg_len = 0;
		for (size_t i = 0; i < tgp->pop_size; i++) {
			avg_fit += tgp->fitness[i];
			avg_len += tgp_len(tgp, i);
		}
		avg_fit /= tgp->pop_size;
		avg_len /= tgp->pop_size;

		// Get the best individual
		size_t best     = tgp_best(tgp);
		double best_fit = tgp_fitness(tgp, best);

		printf(
		    "gen = %4d | avg_fit = %14.2f | best_fit = %14.6f | avg_len = "
		    "%4zu\n",
		    i,
		    avg_fit,
		    best_fit,
		    avg_len
		);

		// Finish if the best individual is perfect
		if (best_fit > -1e-6) {
			break;
		}
	}

	printf("Best individual:\n");
	char str[4096];
	tgp_str(tgp, tgp_best(tgp), 2, str, 4096);
	printf("%s\n", str);

	tgp_free(tgp);
	return 0;
}
