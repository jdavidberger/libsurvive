set -o xtrace

choco upgrade dotnetcore
choco install python
python -m ensurepip

choco install visualstudio2019community
choco install visualstudio2019-workload-nativedesktop



