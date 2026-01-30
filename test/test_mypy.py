"""Test mypy type checking on the source and test folders."""
import subprocess
import sys


def test_mypy():
    """Run mypy on the source and test folders."""
    result = subprocess.run(
        [sys.executable, '-m', 'mypy', '--install-types', '--non-interactive', 'pyvirtualcam', 'test'],
        capture_output=True,
        text=True
    )
    assert result.returncode == 0, f"mypy failed:\n{result.stdout}\n{result.stderr}"
