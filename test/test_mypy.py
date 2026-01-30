"""Test mypy type checking on the source and test folders."""
import subprocess
import sys


def test_mypy_source():
    """Run mypy on the source folder."""
    result = subprocess.run(
        [sys.executable, '-m', 'mypy', '--install-types', '--non-interactive', 'pyvirtualcam'],
        capture_output=True,
        text=True
    )
    assert result.returncode == 0, f"mypy failed on source folder:\n{result.stdout}\n{result.stderr}"


def test_mypy_tests():
    """Run mypy on the test folder."""
    result = subprocess.run(
        [sys.executable, '-m', 'mypy', '--install-types', '--non-interactive', 'test'],
        capture_output=True,
        text=True
    )
    assert result.returncode == 0, f"mypy failed on test folder:\n{result.stdout}\n{result.stderr}"
