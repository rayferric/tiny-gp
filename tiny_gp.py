import os
import ctypes
from ctypes import c_void_p, c_size_t, c_double, c_uint32, c_char_p

libs = {}


def load_lib(name="tiny_gp"):
    global libs

    # Check if the shared library is already loaded
    if name in libs:
        return libs[name]

    # Determine the shared library filename
    if os.name == "posix":
        filename = f"lib{name}.so"
    else:
        filename = f"{name}.dll"

    # Load the shared library
    lib = ctypes.CDLL(os.path.join(os.path.dirname(__file__), "build", filename))

    # Define the function prototypes
    lib.tgp_init.argtypes = [
        c_char_p,
        c_size_t,
        c_size_t,
        c_double,
        c_double,
        c_size_t,
        c_uint32,
    ]
    lib.tgp_init.restype = c_void_p
    lib.tgp_evolve.argtypes = [c_void_p]
    lib.tgp_best.argtypes = [c_void_p]
    lib.tgp_best.restype = c_size_t
    lib.tgp_fitness.argtypes = [c_void_p, c_size_t]
    lib.tgp_fitness.restype = c_double
    lib.tgp_len.argtypes = [c_void_p, c_size_t]
    lib.tgp_len.restype = c_size_t
    lib.tgp_str.argtypes = [c_void_p, c_size_t, c_uint32, c_char_p, c_size_t]
    lib.tgp_free.argtypes = [c_void_p]

    # Save the library
    libs[name] = lib
    return libs[name]


class TinyGP:
    def __init__(
        self,
        path,
        pop_size=10000,
        prog_size=100,
        cmd_mut_prob=0.05,
        cross_vs_mut_prob=0.9,
        tournament_size=2,
        seed=42,
        lib_name="tiny_gp",
    ):
        self.path = path
        self.pop_size = pop_size
        self.prog_size = prog_size
        self.cmd_mut_prob = cmd_mut_prob
        self.cross_vs_mut_prob = cross_vs_mut_prob
        self.tournament_size = tournament_size
        self.seed = seed

        # Load the library
        self.lib = load_lib(lib_name)

        self.tgp = self.lib.tgp_init(
            path.encode("utf-8"),
            pop_size,
            prog_size,
            cmd_mut_prob,
            cross_vs_mut_prob,
            tournament_size,
            seed,
        )

        if not self.tgp:
            raise ValueError("Failed to load the problem.")

    def __del__(self):
        self.lib.tgp_free(self.tgp)

    def evolve(self):
        return self.lib.tgp_evolve(self.tgp)

    def best(self):
        return self.lib.tgp_best(self.tgp)

    def fitness(self, program):
        return self.lib.tgp_fitness(self.tgp, program)

    def len(self, program):
        return self.lib.tgp_len(self.tgp, program)

    def str(self, program, float_precision=2, buf_size=4096):
        buf = ctypes.create_string_buffer(buf_size)
        self.lib.tgp_str(
            self.tgp,
            program,
            float_precision,
            buf,
            buf_size,
        )
        return buf.value.decode("utf-8")
