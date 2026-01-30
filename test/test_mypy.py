"""Test mypy type checking on the source and test folders."""
import subprocess
import sys
from pathlib import Path


def test_mypy():
    """Run mypy on the source and test folders."""
    # Build absolute paths based on test file location
    test_dir = Path(__file__).parent
    repo_root = test_dir.parent
    
    result = subprocess.run(
        [sys.executable, '-m', 'mypy', '--install-types', '--non-interactive', 
         'pyvirtualcam/', 'test/'],
        capture_output=True,
        text=True,
        cwd=str(repo_root)
    )
    assert result.returncode == 0, f"mypy failed:\n{result.stdout}\n{result.stderr}"
