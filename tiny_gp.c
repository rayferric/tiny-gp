#include "tiny_gp.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The program is a sequence of bytes.
// Each byte represents either a function, a variable, or a constant.
// E.g.: y = x0 + 7 * x1 ==> [ADD, 0, MUL, 2, 1]
// For values = [x0, x1, 7]
// here x0 and x1 are variables, 0 and 2 are constants, ADD and MUL are
// functions

const double ZERO_DIV_EPSILON  = 1e-3; // Epsilon for division by zero
const double ZERO_DIV_FALLBACK = 1e6;  // Fallback value for division by zero

const uint8_t F_BEGIN       = 0;
const uint8_t F_BEGIN_UNARY = 0;
#ifdef NO_SIN_COS
const uint8_t F_BEGIN_BIN = 0;
#else
const uint8_t SIN         = F_BEGIN_UNARY + 0;
const uint8_t COS         = F_BEGIN_UNARY + 1;
const uint8_t F_BEGIN_BIN = COS + 1;
#endif
const uint8_t ADD   = F_BEGIN_BIN + 0;
const uint8_t SUB   = F_BEGIN_BIN + 1;
const uint8_t MUL   = F_BEGIN_BIN + 2;
const uint8_t DIV   = F_BEGIN_BIN + 3;
const uint8_t F_END = DIV + 1;

// Evolution Helpers

// LCG RNG
// Replaces global rand() with a custom configurable RNG
uint32_t _tgp_rand(tgp_state *tgp) {
	tgp->rand_state  = tgp->rand_state * 0x5DEECE66DL + 0xBL;
	tgp->rand_state &= (1L << 48) - 1;
	return tgp->rand_state >> (48 - 32);
}

// Used internally by tgp_eval()
// Requires iterators: i_program=?, i_example=?, i_cmd=0
double _tgp_eval_impl(tgp_state *tgp) {
	uint8_t cmd = tgp->programs[tgp->i_program][tgp->i_cmd++];

	// Check if instruction is a var/const
	if (cmd < tgp->num_vars + tgp->num_consts) {
		if (cmd < tgp->num_vars) {
			return tgp->example_vars[tgp->i_example][cmd];
		} else {
			return tgp->consts[cmd - tgp->num_vars];
		}
	}

	// Otherwise, evaluate the operation recursively
	double arg1 = _tgp_eval_impl(tgp);

	size_t i_func = cmd - tgp->num_vars - tgp->num_consts;
	if (i_func < F_BEGIN_BIN) {
		// Unary functions
#ifdef NO_SIN_COS
		assert(false && "Unary functions are not implemented");
#else
		if (i_func == SIN) {
			return sin(arg1);
		}
		if (i_func == COS) {
			return cos(arg1);
		}

		assert(false && "Invalid instruction");
#endif
	} else {
		// Evaluate the second arg
		double arg2 = _tgp_eval_impl(tgp);

		// Binary functions
		if (i_func == ADD) {
			return arg1 + arg2;
		}
		if (i_func == SUB) {
			return arg1 - arg2;
		}
		if (i_func == MUL) {
			return arg1 * arg2;
		}
		if (i_func == DIV) {
			if (fabs(arg2) < ZERO_DIV_EPSILON) {
				if (signbit(arg2)) {
					return -ZERO_DIV_FALLBACK;
				} else {
					return ZERO_DIV_FALLBACK;
				}
			}
			return arg1 / arg2;
		}

		assert(false && "Invalid instruction");
	}
	return 0.0; // unreachable
}

// Evaluates a program (expression) with vars from the example.
double _tgp_eval(tgp_state *tgp, size_t i_program, size_t i_example) {
	tgp->i_program = i_program;
	tgp->i_example = i_example;
	tgp->i_cmd     = 0;
	return _tgp_eval_impl(tgp);
}

// Used internally by tgp_next()
// Requires iterators: i_program=?, i_cmd=0
void _tgp_next_impl(tgp_state *tgp) {
	uint8_t cmd = tgp->programs[tgp->i_program][tgp->i_cmd++];

	// If this is a var/const, return the next node.
	if (cmd < tgp->num_vars + tgp->num_consts) {
		return;
	}

	// Otherwise, skip the children of the operation.
	uint8_t i_func = cmd - tgp->num_vars - tgp->num_consts;
	if (i_func < F_BEGIN_BIN) {
		// Unary functions
		_tgp_next_impl(tgp);
	} else {
		// Binary functions
		_tgp_next_impl(tgp);
		_tgp_next_impl(tgp);
	}
}

// Skips the current node and its children.
// Returns the index of the next node.
// Can be used to find the length of i_program with i_cmd = 0.
size_t _tgp_next(tgp_state *tgp, size_t i_program, size_t i_cmd) {
	tgp->i_program = i_program;
	tgp->i_cmd     = i_cmd;
	_tgp_next_impl(tgp);
	return tgp->i_cmd;
}

// Used internally by tgp_rand_program()
// Requires iterators: i_program=?, i_cmd=?
void _tgp_rand_program_impl(tgp_state *tgp) {
	// If the program is full, return without adding anything
	if (tgp->i_cmd >= tgp->prog_size) {
		return;
	}

	// Select whether to create a terminal or a function
	uint8_t *prog = tgp->programs[tgp->i_program];
	if (_tgp_rand(tgp) % 2) {
		// If terminal, select a random variable or constant
		prog[tgp->i_cmd++] = _tgp_rand(tgp) % (tgp->num_vars + tgp->num_consts);
	} else {
		// Otherwise, select a random function
		uint8_t i_func     = _tgp_rand(tgp) % (F_END - F_BEGIN) + F_BEGIN;
		prog[tgp->i_cmd++] = i_func + tgp->num_vars + tgp->num_consts;
		// If unary, recurse once, otherwise twice
		if (i_func < F_BEGIN_BIN) {
			_tgp_rand_program_impl(tgp);
		} else {
			_tgp_rand_program_impl(tgp);
			_tgp_rand_program_impl(tgp);
		}
	}
}

// Randomizes i_program with a program of length < prog_size
void _tgp_rand_program(tgp_state *tgp, size_t i_program) {
	tgp->i_program = i_program;

	// Keep regenerating programs until one below the length limit is found.
	do {
		tgp->i_cmd = 0;
		_tgp_rand_program_impl(tgp);
	} while (tgp->i_cmd >= tgp->prog_size);
}

void _tgp_str_impl(
    tgp_state *tgp, uint32_t f_precision, char *out, size_t buf_size
) {
	uint8_t cmd = tgp->programs[tgp->i_program][tgp->i_cmd++];

	// If this is a var/const, stringify it
	if (cmd < tgp->num_vars + tgp->num_consts) {
		if (cmd < tgp->num_vars) {
			snprintf(out, buf_size, "X%d", cmd + 1);
		} else {
			snprintf(
			    out,
			    buf_size,
			    "%.*lf",
			    (int)f_precision,
			    tgp->consts[cmd - tgp->num_vars]
			);
		}
		return;
	}

	// Otherwise, stringify the operation recursively
	size_t i_func = cmd - tgp->num_vars - tgp->num_consts;
	if (i_func < F_BEGIN_BIN) {
		// Unary functions
#ifdef NO_SIN_COS
		assert(false && "Unary functions are not implemented");
#else
		const char *fmts[] = {"sin(%s)", "cos(%s)"};
		const char *fmt    = fmts[i_func - F_BEGIN_UNARY];
		char        arg[4096];
		_tgp_str_impl(tgp, f_precision, arg, buf_size);
		snprintf(out, buf_size, fmt, arg);
#endif
	} else {
		// Binary functions
		const char *fmts[] = {
		    "(%s + %s)", "(%s - %s)", "(%s * %s)", "(%s / %s)"
		};
		const char *fmt = fmts[i_func - F_BEGIN_BIN];
		char        arg1[4096], arg2[4096];
		_tgp_str_impl(tgp, f_precision, arg1, buf_size);
		_tgp_str_impl(tgp, f_precision, arg2, buf_size);
		snprintf(out, buf_size, fmt, arg1, arg2);
	}
}

// Exchanges two random subtrees between parents
// and picks one of the parents as offspring.
void _tgp_crossover(tgp_state *tgp, size_t p1, size_t p2, size_t o) {
	uint8_t *prog1 = tgp->programs[p1];
	uint8_t *prog2 = tgp->programs[p2];

	// Compute lengths of the programs
	size_t len1 = _tgp_next(tgp, p1, 0);
	size_t len2 = _tgp_next(tgp, p2, 0);

	// Select two random SUB-TREES from the programs
	size_t st1_b = _tgp_rand(tgp) % len1;
	size_t st1_e = _tgp_next(tgp, p1, st1_b);
	size_t st2_b = _tgp_rand(tgp) % len2;
	size_t st2_e = _tgp_next(tgp, p2, st2_b);

	// Verify that the offspring will not be too long
	// size_t expect_len = 0;
	if (st1_b + (st2_e - st2_b) + (len1 - st1_e) >= tgp->prog_size) {
		uint8_t *tmp_prog = prog1;
		prog1             = prog2;
		prog2             = tmp_prog;

		size_t tmp = len1;
		len1       = len2;
		len2       = tmp;

		tmp   = st1_b;
		st1_b = st2_b;
		st2_b = tmp;

		tmp   = st1_e;
		st1_e = st2_e;
		st2_e = tmp;
	}

	// The following memcpy calls will replace p1 subtree with p2 subtree to
	// create the offspring:

	// From parent 1, copy everything prior to the first subtree
	memcpy(tgp->tmp_program, prog1, st1_b * sizeof(uint8_t));

	// From parent 2, copy the second subtree
	// Effectively replacing the first subtree
	memcpy(
	    tgp->tmp_program + st1_b,
	    prog2 + st2_b,
	    (st2_e - st2_b) * sizeof(uint8_t)
	);

	// From parent 1, copy everything after the first subtree
	memcpy(
	    tgp->tmp_program + st1_b + (st2_e - st2_b),
	    prog1 + st1_e,
	    (len1 - st1_e) * sizeof(uint8_t)
	);

	// Swap output program with the crossover buffer
	uint8_t *tmp     = tgp->programs[o];
	tgp->programs[o] = tgp->tmp_program;
	tgp->tmp_program = tmp;
}

// Mutates the program by changing some of the instructions.
// Each instruction has a cmd_mut_prob chance of being mutated
// into a random instruction of the same type.
void _tgp_mutation(tgp_state *tgp, size_t i_program) {
	// Compute the length of the program
	size_t len = _tgp_next(tgp, i_program, 0);

	// Mutate every instruction in the program
	uint8_t *prog = tgp->programs[i_program];
	for (size_t i = 0; i < len; i++) {
		// Mutate only if lucky
		if (_tgp_rand(tgp) / (double)UINT32_MAX < tgp->cmd_mut_prob) {
			// See if mutating a terminal or a function
			if (prog[i] < tgp->num_vars + tgp->num_consts) {
				// Set the new value to a random variable or constant.
				prog[i] = _tgp_rand(tgp) % (tgp->num_vars + tgp->num_consts);
			} else {
				// Change the function to a random one
				size_t i_func = prog[i] - tgp->num_vars - tgp->num_consts;

				// See if unary or binary
				if (i_func < F_BEGIN_BIN) {
					// Unary functions
#ifdef NO_SIN_COS
					assert(false && "Unary functions are not implemented");
#else
					prog[i] = _tgp_rand(tgp) % (F_BEGIN_BIN - F_BEGIN_UNARY) +
					          F_BEGIN_UNARY;
#endif
				} else {
					// Binary functions

					prog[i] =
					    _tgp_rand(tgp) % (F_END - F_BEGIN_BIN) + F_BEGIN_BIN;
				}
			}
		}
	}
}

// Compute fitness of the currently bound program a sum of absolute
// differences, negated to make it a maximization problem.
void _tgp_compute_fitness(tgp_state *tgp, size_t i_program) {
	double fit = 0.0;

	// Evaluate the program for each example
	for (size_t i = 0; i < tgp->num_examples; i++) {
		double result    = _tgp_eval(tgp, i_program, i);
		double expected  = tgp->example_rets[i];
		fit             += fabs(result - expected);
	}

	// Store the fitness
	tgp->fitness[i_program] = -fit;
}

// tournament_size candidates compete to become a parent.
// Returns the index of the best candidate.
size_t _tgp_tournament(tgp_state *tgp) {
	// Pick a random individual as the best
	size_t best = _tgp_rand(tgp) % tgp->pop_size;

	for (size_t i = 0; i < tgp->tournament_size; i++) {
		size_t competitor = _tgp_rand(tgp) % tgp->pop_size;
		double fit        = tgp->fitness[competitor];
		double best_fit   = tgp->fitness[best];
		if (fit > best_fit) {
			best_fit = fit;
			best     = competitor;
		}
	}

	return best;
}

// tournament_size candidates compete to not be replaced with offspring.
// Returns the index of the worst candidate.
size_t _tgp_negative_tournament(tgp_state *tgp) {
	// Pick a random individual as the worst
	size_t worst = _tgp_rand(tgp) % tgp->pop_size;

	for (size_t i = 0; i < tgp->tournament_size; i++) {
		size_t competitor = _tgp_rand(tgp) % tgp->pop_size;
		double fit        = tgp->fitness[competitor];
		double worst_fit  = tgp->fitness[worst];
		if (fit < worst_fit) {
			worst_fit = fit;
			worst     = competitor;
		}
	}

	return worst;
}

// Public API

// Allocates a new tgp_state and resets its members to NULLs.
// Reads problem definition from a file.
// Populates some fields of the tgp_state.
// Allocates memory to store the examples.
// Initializes the evolution process.
// Allocates memory for programs and fitness.
tgp_state *tgp_init(
    const char *path,
    size_t      pop_size,
    size_t      prog_size,
    double      cmd_mut_prob,
    double      cross_vs_mut_prob,
    size_t      tournament_size,
    uint32_t    seed
) {
	// Start loading the problem
	FILE *f = fopen(path, "r");
	if (!f) {
		return NULL;
	}

	// --- Allocate the state ---

	tgp_state *tgp = malloc(sizeof(tgp_state));
	memset(tgp, 0, sizeof(tgp_state));

	// --- Load the problem ---

	// Read the first line
	fscanf(
	    f,
	    "%zu %zu %lf %lf %zu",
	    &(tgp->num_vars),
	    &(tgp->num_consts),
	    &(tgp->min_rand),
	    &(tgp->max_rand),
	    &(tgp->num_examples)
	);

	// (Re-)allocate memory for examples
	free(tgp->example_vars);
	free(tgp->example_rets);
	tgp->example_vars = malloc(tgp->num_examples * sizeof(double[256]));
	tgp->example_rets = malloc(tgp->num_examples * sizeof(double));

	// Read the rest of the lines
	for (size_t i = 0; i < tgp->num_examples; i++) {
		for (size_t j = 0; j < tgp->num_vars; j++) {
			fscanf(f, "%lf", &(tgp->example_vars[i][j]));
		}
		fscanf(f, "%lf", &tgp->example_rets[i]);
	}

	// Close the file
	fclose(f);

	// --- Initialize evolution state ---

	// Set fields
	tgp->pop_size          = pop_size;
	tgp->prog_size         = prog_size;
	tgp->cmd_mut_prob      = cmd_mut_prob;
	tgp->cross_vs_mut_prob = cross_vs_mut_prob;
	tgp->tournament_size   = tournament_size;
	tgp->rand_state = (((uint64_t)seed) ^ 0x5DEECE66DL) & ((1L << 48) - 1);

	// Allocate programs
	if (tgp->programs) {
		for (size_t i = 0; i < tgp->pop_size; i++) {
			free(tgp->programs[i]);
		}
		free(tgp->programs);
		free(tgp->fitness);
	}
	tgp->programs = malloc(pop_size * sizeof(uint8_t *));
	for (size_t i = 0; i < pop_size; i++) {
		tgp->programs[i] = malloc(prog_size * sizeof(uint8_t));
	}
	tgp->fitness = malloc(pop_size * sizeof(double));

	// Initialize constants
	for (size_t i = 0; i < tgp->num_consts; i++) {
		double o1      = _tgp_rand(tgp) / (double)UINT32_MAX;
		double value   = (tgp->max_rand - tgp->min_rand) * o1 + tgp->min_rand;
		tgp->consts[i] = value;
	}

	// Initialize programs
	for (size_t i = 0; i < pop_size; i++) {
		_tgp_rand_program(tgp, i);
		_tgp_compute_fitness(tgp, i);
	}

	// Allocate tmp program buffer for mutation and crossover
	free(tgp->tmp_program);
	tgp->tmp_program = malloc(prog_size * sizeof(uint8_t));

	return tgp;
}

// Evolves the population for 1 generation
void tgp_evolve(tgp_state *tgp) {
	// Give each individual a chance to reproduce
	for (size_t i = 0; i < tgp->pop_size; i++) {
		// Pick the offspring
		size_t o = _tgp_negative_tournament(tgp);

		// Decide whether to mutate or crossover
		double p = _tgp_rand(tgp) / (double)UINT32_MAX;
		if (p < tgp->cross_vs_mut_prob) {
			// Keep selecting two parents until they are different
			// from each other and the offspring.
			size_t p1, p2;
			do {
				p1 = _tgp_tournament(tgp);
				p2 = _tgp_tournament(tgp);
			} while (p1 == p2 || p1 == o || p2 == o);

			// Cross-over two parents to create a new offspring
			_tgp_crossover(tgp, p1, p2, o);
		} else {
			// Keep selecting a parent until it is different from the offspring.
			size_t p;
			do {
				p = _tgp_tournament(tgp);
			} while (p == o);

			// Overwrite offspring with the best candidate
			memcpy(
			    tgp->programs[o],
			    tgp->programs[p],
			    tgp->prog_size * sizeof(uint8_t)
			);

			// Mutate instructions of one of the best individuals.
			_tgp_mutation(tgp, o);
		}

		// Compute the fitness of the offspring
		_tgp_compute_fitness(tgp, o);
	}
}

// Returns the index of the best individual in the population.
size_t tgp_best(tgp_state *tgp) {
	size_t best = 0;
	for (size_t i = 1; i < tgp->pop_size; i++) {
		if (tgp->fitness[i] > tgp->fitness[best]) {
			best = i;
		}
	}
	return best;
}

// Returns the fitness of the i_program-th individual.
double tgp_fitness(tgp_state *tgp, size_t i_program) {
	return tgp->fitness[i_program];
}

// Returns the length of the i_program-th individual.
size_t tgp_len(tgp_state *tgp, size_t i_program) {
	return _tgp_next(tgp, i_program, 0);
}

// Converts a program to a string representation.
// The string is written to the out buffer.
void tgp_str(
    tgp_state *tgp,
    size_t     i_program,
    uint32_t   f_precision,
    char      *out,
    size_t     buf_size
) {
	tgp->i_program = i_program;
	tgp->i_cmd     = 0;
	_tgp_str_impl(tgp, f_precision, out, buf_size);
	out[strlen(out)] = '\0';
}

// Frees the memory allocated by any tgp_ functions.
// tgp pointer is freed as well and must be re-allocated using tgp_init.
void tgp_free(tgp_state *tgp) {
	if (!tgp) {
		return;
	}

	free(tgp->tmp_program);
	for (size_t i = 0; i < tgp->pop_size; i++) {
		free(tgp->programs[i]);
	}
	free(tgp->programs);
	free(tgp->example_rets);
	free(tgp->example_vars);
	free(tgp);
}
