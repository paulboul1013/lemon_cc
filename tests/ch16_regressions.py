#!/usr/bin/env python3
"""Exercise character and string semantics through the compiler CLI."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest


class Chapter16(unittest.TestCase):
    compiler = None

    def compile(self, source, stage=None):
        path = Path(self.directory.name) / 'case.c'
        path.write_text(source)
        args = [str(self.compiler)] + ([stage] if stage else []) + [str(path)]
        return subprocess.run(args, capture_output=True, text=True, timeout=10), path

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='lemon-ch16-')
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

    def test_character_promotions_and_byte_storage(self):
        self.runs_zero("""
            int main(void) {
                signed char a = -1;
                unsigned char b = 255;
                char c[3] = {1, 2, 3};
                if (a + b != 254 || b + b != 510 || ~b != -256) return 1;
                b >>= 2;
                if (b != 63 || c[1] != 2 || c[2] != 3) return 2;
                return !(a < 0 && (long)a == -1 && (double)b == 63.0);
            }
        """)

    def test_character_constants_define_array_dimensions(self):
        self.runs_zero(r"""
            int main(void) {
                int a['\t'] = {1, 2, 3};
                int (*p)[9] = &a;
                return !((*p)[2] == 3 && a[8] == 0);
            }
        """)

    def test_string_arrays_static_pointers_and_literal_addresses(self):
        self.runs_zero(r"""
            static char exact[3] = "abc";
            static unsigned char padded[6] = "ab" "c";
            static char *p = "hello";
            int main(void) {
                char local[5] = "abc";
                signed char nested[2][4] = {"ab", "c"};
                char (*whole)[4] = &"abc";
                if ('a' != 97 || '\n' != 10 || '\\' != 92) return 1;
                if (exact[2] != 'c' || padded[5] || local[4]) return 2;
                if (nested[0][2] || nested[1][0] != 'c' || (*whole)[3]) return 3;
                local[0] = 'x';
                return !(local[0] == 'x' && p[4] == 'o' && ""[0] == 0);
            }
        """)

    def test_string_object_names_cannot_shadow_user_symbols(self):
        self.runs_zero('int string_0 = 11; char *message = "a"; int main(void) { return !(string_0 == 11 && message[0] == 97); }')

    def test_character_updates_evaluate_the_lvalue_once_and_narrow_the_result(self):
        self.runs_zero("""
            int visits;
            unsigned char data[2] = {255, 19};
            unsigned char *select(void) { visits += 1; return data; }
            int main(void) {
                unsigned char before = (*select())++;
                if (visits != 1 || before != 255 || data[0] != 0) return 1;
                unsigned char after = --(*select());
                if (visits != 2 || after != 255 || data[0] != 255) return 2;
                int assigned = (*select()) *= 2;
                if (visits != 3 || assigned != 254 || data[0] != 254 || data[1] != 19) return 3;
                signed char negative = -8;
                negative >>= 2;
                if (negative != -2 || -negative != 2) return 4;
                switch (data[0]) { case -2: return 5; case 254: return 0; default: return 6; }
            }
        """)

    def test_lex_rejects_malformed_and_unsupported_literals(self):
        literals = ["''", "'ab'", r"'\0'", r"'\x41'", '"unfinished', '"line\nbreak"', r'"bad\z"']
        for literal in literals:
            with self.subTest(literal=literal):
                result, _ = self.compile('int main(void) { return ' + literal + '; }', '--lex')
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertIn('Lexer Error:', result.stderr)

    def test_validate_rejects_string_capacity_and_character_pointer_conflicts(self):
        sources = [
            'char value; signed char value;',
            'int main(void) { unsigned char *p = "a"; return 0; }',
            'int main(void) { char a[2] = "abc"; return 0; }',
            'static signed char a[2][2] = {"a", "abc"};',
            'static int a[3] = "ab";',
        ]
        for source in sources:
            with self.subTest(source=source):
                self.rejects(source)

    def test_assembler_preserves_every_supported_escape(self):
        self.runs_zero(r"""
            static char text[12] = "\a\b\f\n\r\t\v\\\"\'\?";
            int main(void) {
                int expected[12] = {7, 8, 12, 10, 13, 9, 11, 92, 34, 39, 63, 0};
                for (int i = 0; i < 12; i += 1) if (text[i] != expected[i]) return i + 1;
                return 0;
            }
        """)

    def test_gcc_optimized_character_argument_and_return_interoperability(self):
        provider = """
            signed char narrow(long value) { return (signed char)value; }
            unsigned char high(void) { return 251; }
            int check(char a, unsigned char b, signed char c, char d, unsigned char e,
                      signed char f, char g, unsigned char h, signed char i) {
                return !(a == -3 && b == 255 && c == -128 && d == 63 && e == 129 &&
                         f == -7 && g == -9 && h == 250 && i == -11);
            }
        """
        caller = """
            signed char narrow(long value); unsigned char high(void);
            int check(char a, unsigned char b, signed char c, char d, unsigned char e,
                      signed char f, char g, unsigned char h, signed char i);
            int main(void) {
                signed char s = -128; unsigned char u = 255;
                if (narrow(511l) != -1 || high() != 251) return 1;
                return check(-3, u, s, 63, 129, -7, -9, 250, -11);
            }
        """
        directory = Path(self.directory.name)
        for lemon_provider in [False, True]:
            with self.subTest(lemon_provider=lemon_provider):
                lemon = directory / 'lemon.c'
                gcc = directory / 'gcc.c'
                lemon.write_text(provider if lemon_provider else caller)
                gcc.write_text(caller if lemon_provider else provider)
                commands = [
                    [str(self.compiler), '-c', str(lemon)],
                    ['gcc', '-O2', '-std=c17', '-fsigned-char', '-c', str(gcc), '-o', str(gcc.with_suffix('.o'))],
                    ['gcc', str(lemon.with_suffix('.o')), str(gcc.with_suffix('.o')), '-o', str(directory / 'abi')],
                    [str(directory / 'abi')],
                ]
                for command in commands:
                    result = subprocess.run(command, capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compiler', type=Path)
    args, rest = parser.parse_known_args()
    Chapter16.compiler = args.compiler.resolve()
    unittest.main(argv=['ch16_regressions.py'] + rest)
