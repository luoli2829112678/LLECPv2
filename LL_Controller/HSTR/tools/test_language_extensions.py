"""End-to-end ST -> C -> shared module -> Runtime.Cycle regression tests.
Usage: python3 tools/test_language_extensions.py --build /absolute/HSTR-build
Requires an already built HSTR.so and a C++17 compiler. Uses temporary applications.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = args.build.resolve()
    fixtures = root / 'tests/language'
    with tempfile.TemporaryDirectory(prefix='hstr-language-') as tmp:
        tmp = Path(tmp)
        driver = tmp / 'driver'
        subprocess.run(['g++', '-std=c++17', str(fixtures/'driver.cpp'),
                        '-I'+str(root/'include'), '-I'+str(build/'generated/include'),
                        str(build/'HSTR.so'), '-Wl,-rpath,'+str(build), '-o', str(driver)], check=True)
        cases = [
            ('positive', (fixtures/'positive.st').read_text(), None),
            ('numeric_boundaries', '''PROGRAM Main
VAR
  Big : ULINT := ULINT#18446744073709551600;
  CopyBig : ULINT; Mask : LWORD;
  Negative : INT := -7; Wide : LINT; Small : SINT;
  Positive : UINT := 123; Signed : DINT := -5; Sum : LINT;
  X : LREAL := -2.5; Rounded : INT;
  Flag : BOOL := TRUE; FlagInt : INT;
  F : LREAL; OK : BOOL;
END_VAR
CopyBig := Big; Mask := Big; Wide := Negative; Small := Negative;
Sum := Positive + Signed; Rounded := X; FlagInt := Flag;
F := Echo(Value := Negative);
OK := (CopyBig = Big) AND (Mask = LWORD#18446744073709551600)
 AND (Wide = -7) AND (Small = -7) AND (Sum = 118)
 AND (Rounded = -2) AND (FlagInt = 1) AND (F = -7.0);
END_PROGRAM''', None),
            ('sizeof_types', '''PROGRAM Main
VAR A : Numbers; Rec : Pair; Text : STRING; Idx : INT := 1; S1,S2,S3,S4,S5,S6,S7,S8,S9 : ULINT; OK : BOOL; END_VAR
S1 := SIZEOF(BOOL); S2 := SIZEOF(LINT); S3 := SIZEOF(CountType);
S4 := SIZEOF(Numbers); S5 := SIZEOF(A); S6 := SIZEOF(Rec.Small);
S7 := SIZEOF(A[Idx]); S8 := SIZEOF(Text); S9 := SIZEOF(STRING);
OK := (SIZEOF(BOOL) = 1) AND (SIZEOF(LINT) = 8)
 AND (SIZEOF(CountType) = 4) AND (SIZEOF(Numbers) = SIZEOF(A))
 AND (SIZEOF(Rec.Small) = 1) AND (SIZEOF(A[Idx]) = 2)
 AND (SIZEOF(Text) = SIZEOF(STRING));
END_PROGRAM''', None),
            ('identifier_regression', '''PROGRAM Main
VAR DSIZEOF : INT := 3; OK : BOOL; END_VAR
OK := DSIZEOF = 3;
END_PROGRAM''', None),
            ('string_to_int', "PROGRAM Main VAR X : STRING := 'abc'; Y : INT; OK : BOOL; END_VAR Y := X; OK := TRUE; END_PROGRAM", 'Incompatible'),
            ('struct_to_int', 'PROGRAM Main VAR X : Pair; Y : INT; OK : BOOL; END_VAR Y := X; OK := TRUE; END_PROGRAM', 'Incompatible'),
            ('array_to_int', 'PROGRAM Main VAR X : Numbers; Y : INT; OK : BOOL; END_VAR Y := X; OK := TRUE; END_PROGRAM', 'Incompatible'),
            ('inout_mismatch', 'PROGRAM Main VAR X : INT; OK : BOOL; END_VAR OK := UpdateValue(X); END_PROGRAM', 'type'),
            ('bad_input', "PROGRAM Main VAR X : STRING; Y : LREAL; OK : BOOL; END_VAR Y := Echo(X); OK := TRUE; END_PROGRAM", 'type'),
            ('sizeof_unknown', 'PROGRAM Main VAR OK : BOOL; END_VAR OK := SIZEOF(MissingName) = 4; END_PROGRAM', 'error'),
            ('sizeof_empty', 'PROGRAM Main VAR OK : BOOL; END_VAR OK := SIZEOF() = 4; END_PROGRAM', 'error'),
            ('mixed_ulint_signed', 'PROGRAM Main VAR A : ULINT; B : LINT; C : LINT; OK : BOOL; END_VAR C := A + B; OK := TRUE; END_PROGRAM', 'type'),
        ]
        for index, (name, source, expected_error) in enumerate(cases):
            folder = tmp/name
            folder.mkdir()
            for file in ('Types.typ','Helpers.fun'):
                shutil.copy(fixtures/file, folder/file)
            path = folder/'Main.st'
            path.write_text(source)
            result = subprocess.run([str(driver), str(tmp/('app'+str(index))), str(path)], text=True, capture_output=True)
            output = result.stdout+result.stderr
            if expected_error is None:
                assert result.returncode == 0, name+'\n'+output
            else:
                assert result.returncode != 0 and expected_error.lower() in output.lower(), name+'\n'+output
            print('PASS', name)
        print(f'PASS {len(cases)} end-to-end cases')
        # HSTR intentionally does not enable REF syntax; check the compiler's existing -r path separately.
        reference = tmp/'reference.st'
        reference.write_text('PROGRAM Main VAR X : DINT := 9; P : REF_TO DINT; OK : BOOL; END_VAR P := REF(X); X := DREF(P); OK := X = 9; END_PROGRAM')
        out = tmp/'ref-output'
        out.mkdir()
        result = subprocess.run([str(driver), '--hstr-compiler', '-r', '-e', '-p',
                                 '-I', str(root/'third_party/matiec/lib'), '-T', str(out), str(reference)],
                                text=True, capture_output=True)
        assert result.returncode == 0, result.stdout+result.stderr
        print('PASS existing REF/DREF compiler syntax')


if __name__ == '__main__':
    main()
