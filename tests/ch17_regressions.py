#!/usr/bin/env python3
"""Exercise void, void pointers and sizeof through the compiler CLI."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest


class Chapter17(unittest.TestCase):
    compiler = None

    def compile(self, source, stage=None):
        path = Path(self.directory.name) / 'case.c'
        path.write_text(source)
        args = [str(self.compiler)] + ([stage] if stage else []) + [str(path)]
        return subprocess.run(args, capture_output=True, text=True, timeout=10), path

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='lemon-ch17-')
        self.addCleanup(self.directory.cleanup)

    def runs_zero(self, source):
        result, path = self.compile(source)
        self.assertEqual(result.returncode, 0, result.stderr)
        result = subprocess.run([str(path.with_suffix(''))], capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)

    def rejects(self, source, error='Type Error:'):
        result, _ = self.compile(source, '--validate')
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn(error, result.stderr)

    def test_sizeof_does_not_evaluate_its_operand(self):
        self.runs_zero("""
            int counter = 0;
            int bump(void) { counter = counter + 1; return counter; }
            int main(void) {
                unsigned long a = sizeof (counter = 100);
                unsigned long b = sizeof bump();
                return !(a == 4 && b == 4 && counter == 0);
            }
        """)

    def test_sizeof_sizes_expressions_and_types_without_array_decay(self):
        self.runs_zero("""
            int main(void) {
                int array[3];
                char text[2][5];
                double d;
                int *p = array;
                if (sizeof array != 12 || sizeof text != 10 || sizeof text[1] != 5) return 1;
                if (sizeof (char) != 1 || sizeof (unsigned char) != 1 || sizeof (double) != 8) return 2;
                if (sizeof (int *) != 8 || sizeof (long[3][2]) != 48 || sizeof (void *) != 8) return 3;
                if (sizeof d != 8 || sizeof p != 8 || sizeof *p != 4 || sizeof "abc" != 4) return 4;
                if (sizeof ((int) d) != 4 || sizeof 'a' != 4 || sizeof -sizeof (char) != 8) return 5;
                return !(sizeof (char) - 2 > 0);
            }
        """)

    def test_void_functions_conditionals_and_casts(self):
        self.runs_zero("""
            int counter = 0;
            void bump(void) { counter = counter + 1; }
            void bump_twice(void) { bump(); bump(); return; }
            void maybe(int flag) { if (flag) return; counter = counter + 10; }
            int main(void) {
                bump();
                bump_twice();
                (void) bump();
                (void) counter;
                counter ? bump() : bump_twice();
                0 ? bump() : bump_twice();
                maybe(1);
                maybe(0);
                return !(counter == 17);
            }
        """)

    def test_void_pointer_converts_implicitly_in_both_directions(self):
        self.runs_zero("""
            int main(void) {
                int x = 7;
                int *ip = &x;
                void *vp = ip;
                int *back = vp;
                double *dp = vp;
                void *picked = x ? vp : ip;
                if (vp != ip || !(ip == vp) || picked != ip) return 1;
                if ((void *) dp != back || (int *) vp != ip) return 2;
                return !(*back == 7 && !(vp == 0));
            }
        """)

    def test_hand_declared_memory_management_functions(self):
        self.runs_zero("""
            void *malloc(unsigned long size);
            void *calloc(unsigned long count, unsigned long size);
            void *realloc(void *ptr, unsigned long size);
            void free(void *ptr);
            int main(void) {
                int *many = malloc(10 * sizeof (int));
                for (int i = 0; i < 10; i = i + 1) many[i] = i;
                many = realloc(many, 20 * sizeof *many);
                long *zeros = calloc(4, sizeof (long));
                int ok = many[9] == 9 && zeros[3] == 0;
                free(many);
                free(zeros);
                return !ok;
            }
        """)

    def test_validate_rejects_invalid_void_and_incomplete_uses(self):
        sources = [
            'void f(void) { return (void) 1; }',
            'int main(void) { int x; void *p = &x; void *q = &*p; return 0; }',
            'int main(void) { int x; void *p = &x; *p; return 0; }',
            'int main(void) { void x; return 0; }',
            'extern void x;',
            'int f(void a);',
            'int main(void) { return sizeof (void); }',
            'int main(void) { void *p = 0; p = p + 1; return 0; }',
            'int main(void) { void *p = 0; return p - p; }',
            'int main(void) { void *p = 0; p[0]; return 0; }',
            'void arr[3];',
            'void (*p)[3];',
            'int f(void a[3]);',
            'void f(void); int main(void) { int x = 1; x ? f() : 1; return 0; }',
            'int f(void) { return; }',
            'void f(void) { return 1; }',
            'void f(void); int main(void) { return !f(); }',
            'void f(void); int main(void) { if (f()) return 1; return 0; }',
            'void f(void); int main(void) { return f() == f(); }',
            'int main(void) { return (int[2]) 0; }',
        ]
        for source in sources:
            with self.subTest(source=source):
                self.rejects(source)

    def test_parse_rejects_cast_operand_of_sizeof_and_void_combinations(self):
        sources = [
            'int main(void) { int x; return sizeof (int) x; }',
            'void long x;',
            'unsigned void *p;',
            'int f(int a, void);',
        ]
        for source in sources:
            with self.subTest(source=source):
                self.rejects(source, 'Parse Error:')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compiler', type=Path)
    args, rest = parser.parse_known_args()
    Chapter17.compiler = args.compiler.resolve()
    unittest.main(argv=['ch17_regressions.py'] + rest)
