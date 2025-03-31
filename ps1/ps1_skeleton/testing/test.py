import difflib
import pathlib
import os
import subprocess
import sys


def main():
    ps1_root_dir = pathlib.Path(__file__).parent.parent.resolve()

    build_dir = ps1_root_dir / "build"

    print("Building and compiling...")
    try:
        subprocess.run(["cmake", str(ps1_root_dir), "-B", str(build_dir)], check=True)
        subprocess.run(["cmake", "--build", str(build_dir)], check=True)
    except subprocess.CalledProcessError as e:
        print(f"An error occurred while running CMake: {e}", file=sys.stderr)
        sys.exit(1)
    print("Build complete")

    testing_dir = ps1_root_dir / "testing"
    tests_dir = testing_dir / "tests"
    outputs_dir = testing_dir / "expected"

    executable = build_dir / "ps1"

    sorted_test_files : list[pathlib.Path] = []

    for test_file in tests_dir.glob("*.txt"):
        sorted_test_files.append(test_file)
    
    sorted_test_files.sort(key= lambda p : p.name)

    passed = 0
    for test_file in sorted_test_files:
        name = test_file.name
        print(f"Testing {name}... ", end="")
        output_file = outputs_dir / name

        if not output_file.exists():
            print("")
            print(f"Error: Could not find {str(output_file)}", file=sys.stderr)
            sys.exit(1)

        try:
            result : list[str] = []
            with test_file.open("r") as test_input:
                result_str : str = subprocess.run(
                    [executable], 
                    stdin=test_input, 
                    stdout=subprocess.PIPE, 
                    stderr=subprocess.PIPE,
                    text=True,  # Output must be text
                    check=True
                ).stdout

                result = result_str.splitlines()
            
            expected : list[str] = []
            with output_file.open("r") as expected_output:
                expected = expected_output.read().splitlines()
            
            diff_list : list[str] = []
            for diff in difflib.unified_diff(result, expected, fromfile='your output', tofile='expected_output', lineterm='', n=3):
                diff_list.append(diff)

            if not diff_list:
                # List is empty; no diffs
                passed += 1
                print("OK")
                continue

            # If at least one diff, print
            print("MISMATCH")
            for diff in diff_list:
                print(diff)
        
        except subprocess.CalledProcessError as e:
            print("")
            print(f"Error: Test {name} failed to execute: {e}", file=sys.stderr)
    
    print(f"{passed}/{len(sorted_test_files)} tests passed")


if __name__ == "__main__":
    main()
