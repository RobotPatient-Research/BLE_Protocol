# Use UBI9-minimal as base
FROM debian:bookworm-slim

# Install core build & runtime tools
RUN apt-get update && apt-get install -y \
    build-essential \
    flex \
    bison \
    device-tree-compiler \
    python3-pip \
    python3-venv \
    cmake \
    ninja-build \
    git \
    sudo \
    gdb \
    && apt-get clean


# Add non-root user for VS Code Dev Container usage
RUN useradd -m vscode && echo "vscode ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/vscode

RUN pip install dtc west  --break-system-packages

RUN mkdir -p /opt && chmod 0777 /opt

# Switch to dev user
USER vscode
WORKDIR /workspace

RUN sudo chown -R vscode:vscode /opt

# Default command (for devcontainer)
CMD ["sleep", "infinity"]
