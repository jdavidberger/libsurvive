set -o xtrace

choco upgrade dotnetcore
choco install python

export PATH="/c/Python38/:$PATH"
python -m ensurepip

choco install visualstudio2019community
choco install visualstudio2019-workload-nativedesktop



