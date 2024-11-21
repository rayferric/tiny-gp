import time
import sys
from tiny_gp import TinyGP

POP_SIZE = 10000  # Number of programs in a population
PROG_SIZE = 100  # Maximum length of a program
CMD_MUTATION_PROB = 0.05  # Probability of mutation per node
CROSS_VS_MUT_PROB = 0.9  # Probability of crossover between two parents
TOURNAMENT_SIZE = 2  # Tournament size for parent selection


def main():
    path = None
    seed = int(time.time())

    if len(sys.argv) == 3:
        seed = int(sys.argv[1])
        path = sys.argv[2]
    elif len(sys.argv) == 2:
        path = sys.argv[1]
    else:
        print(f"Usage: {sys.argv[0]} [seed?] <path/to/problem.dat>")
        return

    try:
        tgp = TinyGP(
            path,
            POP_SIZE,
            PROG_SIZE,
            CMD_MUTATION_PROB,
            CROSS_VS_MUT_PROB,
            TOURNAMENT_SIZE,
            seed,
        )
    except Exception as e:
        print(e)
        return

    for i in range(100):
        tgp.evolve()

        # Compute avg fitness
        avg_fit = sum(tgp.fitness(j) for j in range(tgp.pop_size)) / tgp.pop_size
        avg_len = sum(tgp.len(j) for j in range(tgp.pop_size)) // tgp.pop_size

        # Get the best individual
        best = tgp.best()
        best_fit = tgp.fitness(best)

        print(
            f"gen = {i:4d} | avg_fit = {avg_fit:14.2f} | best_fit = {best_fit:14.6f} | avg_len = {avg_len:4d}"
        )

        # Finish if the best individual is perfect
        if best_fit > -1e-6:
            break

    print("Best individual:")
    print(tgp.str(tgp.best(), 2))


if __name__ == "__main__":
    main()
