#!/usr/bin/env python3
"""Exercise array and pointer semantics through the compiler CLI."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest


class Chapter15(unittest.TestCase):
    compiler = None

    def compile(self, source, stage=None):
        path = Path(self.directory.name) / 'case.c'
        path.write_text(source)
        args = [str(self.compiler)] + ([stage] if stage else []) + [str(path)]
        return subprocess.run(args, capture_output=True, text=True, timeout=10), path

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='lemon-ch15-')
        self.addCleanup(self.directory.cleanup)

    def runs_zero(self, source):
        result, path = self.compile(source)
        self.assertEqual(result.returncode, 0, result.stderr)
        result = subprocess.run([str(path.with_suffix(''))], capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)

    def rejects(self, source):
        result, _ = self.compile(source, '--validate')
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn('Error:', result.stderr)

    def test_postfix_subscript_and_pointer_updates_evaluate_lvalue_once(self):
        self.runs_zero('''
            int count;
            int a[3][3] = {{1,2,3},{4,5,6},{7,8,9}};
            int (*p)[3];
            int (**pick(void))[3] { count += 1; return &p; }
            int main(void) {
                p = a;
                int (*old)[3] = (*pick())++;
                if (count != 1 || old != a || p != a + 1) return 1;
                (*pick()) += 1;
                if (count != 2 || p != a + 2 || 1[(*p)] != 8) return 2;
                int (*back)[3] = --(*pick());
                if (count != 3 || back != a + 1) return 3;
                return !((*p++)[2] == 6 && p == a + 2 && p - a == 2 && a - p == -2);
            }
        ''')

    def test_nested_zero_fill_and_negative_zero_static_data(self):
        self.runs_zero('''
            static double d[2][2] = {{-0.0}, {2.0}};
            static int a[2][3] = {{7}, {8,9}};
            static int *p[2] = {0};
            int main(void) {
                int b[2][3] = {{7}, {8,9}};
                if (!(1.0 / d[0][0] < 0.0) || d[0][1] != 0.0) return 1;
                return !(a[0][1] == 0 && a[1][2] == 0 && b[1][1] == 9 && b[1][2] == 0 && p[1] == 0);
            }
        ''')

    def test_outer_parameter_dimension_adjustment_and_array_return_pointer(self):
        self.runs_zero('''
            int read(int a[999999999999UL][3]);
            int read(int (*a)[3]) { return a[1][2]; }
            int (*same(int a[2][3]))[3] { return a; }
            int main(void) { int a[2][3] = {{1},{2,3,4}}; return !(read(same(a)) == 4); }
        ''')

    def test_unused_external_huge_array_requires_no_layout(self):
        self.runs_zero('extern int a[18446744073709551615UL][100000000]; int main(void) { return 0; }')

    def test_large_concrete_object_rejected_before_zero_padding(self):
        for source in [
            'int a[2147483647] = {1};',
            'int main(void) { int a[18446744073709551615UL]; return 0; }',
            'int a[1000000000][1000000000];',
        ]:
            with self.subTest(source=source):
                self.rejects(source)

    def test_oversized_pointer_stride_rejected_cleanly(self):
        result, _ = self.compile('int main(void) { int (*p)[1000000000] = 0; p += 1; return 0; }')
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn('Error:', result.stderr)

    def test_integer_compound_assignment_cannot_accept_pointer(self):
        for op in ['+=', '-=']:
            with self.subTest(op=op):
                self.rejects('int main(void) { int a[2]; long x = 0; x ' + op + ' a; return 0; }')

    def test_pointer_compound_assignment_cannot_accept_pointer(self):
        self.rejects('int main(void) { int a[2]; int *p = a; p -= a; return 0; }')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compiler', type=Path)
    args, rest = parser.parse_known_args()
    Chapter15.compiler = args.compiler.resolve()
    unittest.main(argv=['ch15_regressions.py'] + rest)
