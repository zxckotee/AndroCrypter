#!/usr/bin/env bash
# Optional helper: print steps to install a self-hosted runner (run on your SSH server as the CI user).
# Registration token must come from GitHub UI (Settings → Actions → Runners → New).
set -euo pipefail

RUNNER_VERSION="${RUNNER_VERSION:-2.321.0}"
RUNNER_USER="${RUNNER_USER:-hcvault-runner}"
REPO_URL="${REPO_URL:-https://github.com/zxckotee/AndroCrypter}"

cat <<EOF
Install a self-hosted runner for HCVault (labels: self-hosted,linux,kvm,hcvault)

1) On the server:
   sudo useradd -m -s /bin/bash ${RUNNER_USER} || true
   sudo usermod -aG docker,kvm ${RUNNER_USER}

2) GitHub → Settings → Actions → Runners → New self-hosted runner → copy TOKEN

3) As ${RUNNER_USER}:
   sudo su - ${RUNNER_USER}
   mkdir -p ~/actions-runner && cd ~/actions-runner
   curl -fsSL -o actions-runner.tgz \\
     https://github.com/actions/runner/releases/download/v${RUNNER_VERSION}/actions-runner-linux-x64-${RUNNER_VERSION}.tar.gz
   tar xzf actions-runner.tgz
   ./config.sh --url ${REPO_URL} --token <PASTE_TOKEN> \\
     --name \$(hostname)-hcvault --labels self-hosted,linux,kvm,hcvault --unattended
   sudo ./svc.sh install
   sudo ./svc.sh start

4) Add repository secret TEST_VOLUME_PASSWORD (see docs/ci-github.md)

EOF
