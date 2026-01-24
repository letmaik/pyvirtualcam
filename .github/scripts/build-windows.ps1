$ErrorActionPreference = 'Stop'

function exec {
    [CmdletBinding()]
    param([Parameter(Position=0,Mandatory=1)][scriptblock]$cmd)
    Write-Host "$cmd"
    # https://stackoverflow.com/q/2095088
    $ErrorActionPreference = 'Continue'
    & $cmd
    $ErrorActionPreference = 'Stop'
    if ($lastexitcode -ne 0) {
        throw ("ERROR exit code $lastexitcode")
    }
}

function Initialize-Python {
    if ($env:USE_CONDA -eq 1) {
        $env:CONDA_ROOT = $pwd.Path + "\external\miniconda_$env:PYTHON_ARCH"
        & .\.github\scripts\install-miniconda.ps1
        & $env:CONDA_ROOT\shell\condabin\conda-hook.ps1
        exec { conda update --yes -n base -c defaults conda }
    }
    # Check Python version/arch
    exec { python -c "import platform; assert platform.python_version().startswith('$env:PYTHON_VERSION')" }
}

function Create-VEnv {
    [CmdletBinding()]
    param([Parameter(Position=0,Mandatory=1)][string]$name)
    if ($env:USE_CONDA -eq 1) {
        exec { conda create --yes --name $name -c defaults --strict-channel-priority python=$env:PYTHON_VERSION --force }
    } else {
        exec { python -m venv env\$name }
    }
}

function Enter-VEnv {
    [CmdletBinding()]
    param([Parameter(Position=0,Mandatory=1)][string]$name)
    if ($env:USE_CONDA -eq 1) {
        conda activate $name
    } else {
        & .\env\$name\scripts\activate
    }
}

function Create-And-Enter-VEnv {
    [CmdletBinding()]
    param([Parameter(Position=0,Mandatory=1)][string]$name)
    Create-VEnv $name
    Enter-VEnv $name
}

function Exit-VEnv {
    if ($env:USE_CONDA -eq 1) {
        conda deactivate
    } else {
        deactivate
    }
}

if (!$env:PYTHON_VERSION) {
    throw "PYTHON_VERSION env var missing, must be x.y"
}
if ($env:PYTHON_ARCH -ne 'x86' -and $env:PYTHON_ARCH -ne 'x86_64') {
    throw "PYTHON_ARCH env var must be x86 or x86_64"
}
if (!$env:NUMPY_VERSION) {
    throw "NUMPY_VERSION env var missing"
}

Initialize-Python

# Prefer binary packages over building from source
$env:PIP_PREFER_BINARY = 1

Get-ChildItem env:

# Build the wheel.
Create-And-Enter-VEnv build
exec { python -m pip install --upgrade pip wheel setuptools }
exec { python -m pip install --only-binary :all: numpy==$env:NUMPY_VERSION }
exec { python -u setup.py bdist_wheel }
Exit-VEnv
