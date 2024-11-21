#ifndef TINY_GP_H
#define TINY_GP_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
	size_t num_examples, num_vars, num_consts;
	double min_rand, max_rand;
	double (*example_vars)[256];
	double   *example_rets;
	size_t    pop_size, prog_size;
	double    cmd_mut_prob, cross_vs_mut_prob;
	size_t    tournament_size;
	double    consts[256];
	uint8_t **programs;
	double   *fitness;
	uint8_t  *tmp_program;
	uint64_t  rand_state;
	size_t    i_program;
	size_t    i_example;
	size_t    i_cmd;
} tgp_state;

uint32_t _tgp_rand(tgp_state *tgp);
double   _tgp_eval_impl(tgp_state *tgp);
double   _tgp_eval(tgp_state *tgp, size_t i_program, size_t i_example);
void     _tgp_next_impl(tgp_state *tgp);
size_t   _tgp_next(tgp_state *tgp, size_t i_program, size_t i_cmd);
void     _tgp_rand_program_impl(tgp_state *tgp);
void     _tgp_rand_program(tgp_state *tgp, size_t i_program);
void     _tgp_str_impl(
        tgp_state *tgp, uint32_t f_precision, char *out, size_t buf_size
    );
void   _tgp_crossover(tgp_state *tgp, size_t p1, size_t p2, size_t o);
void   _tgp_mutation(tgp_state *tgp, size_t i_program);
void   _tgp_compute_fitness(tgp_state *tgp, size_t i_program);
size_t _tgp_tournament(tgp_state *tgp);
size_t _tgp_negative_tournament(tgp_state *tgp);

// Public API

tgp_state *tgp_init(
    const char *path,
    size_t      pop_size,
    size_t      prog_size,
    double      cmd_mut_prob,
    double      cross_vs_mut_prob,
    size_t      tournament_size,
    uint32_t    seed
);
void   tgp_evolve(tgp_state *tgp);
size_t tgp_best(tgp_state *tgp);
double tgp_fitness(tgp_state *tgp, size_t i_program);
size_t tgp_len(tgp_state *tgp, size_t i_program);
void   tgp_str(
      tgp_state *tgp,
      size_t     i_program,
      uint32_t   f_precision,
      char      *out,
      size_t     buf_size
  );
void tgp_free(tgp_state *tgp);

#endif // TINY_GP_H
